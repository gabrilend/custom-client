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
 * Usage: test-client PROJECT_DIR CLIENT_DATA_DIR [CONVERTED_BUILD_FOLDER [WAR3_MPQ]]
 *   CONVERTED_BUILD_FOLDER  a converted map's build (default
 *                           PROJECT_DIR/assets/test-map-build; "-" for it)
 *   WAR3_MPQ                Warcraft III's war3.mpq, for the archives'
 *                           WC3 checks (default: world-edit-to-execute's
 *                           wc3-installs copy). Each its own argument
 *                           since 1109's step 1e: both were the third.
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
#include "../src/059-loose-folders.h"
#include "../src/060-resolver.h"
#include "../src/067-test-map.h"
#include "../src/068-goodbye.h"
#include "../src/061-mdx.h"
#include "../src/073-converted-map.h"
#include "../src/074-tables.h"
#include "../src/075-adt-heights.h"
#include "../src/076-placed-models.h"
#include "../src/077-slk.h"
#include "../src/078-wc3-ground.h"
#include "../src/079-wc3-cliffs.h"
#include "../src/080-ground-shader.h"
#include "../src/081-wc3-water.h"
#include "../src/082-water-prep.h"
#include "../src/083-w3i.h"
#include "../src/063-w3e.h"

#define RAYMATH_STATIC_INLINE              /* no raylib library in the tests: the maths compiled in here */
#include "raymath.h"

#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#undef W_OK                              /* <unistd.h>'s access() mode (2) would hide 013-errors.h's W_OK (0): every == W_OK below would test for 2 */
#include <time.h>

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
typedef struct { unsigned char b[4096]; size_t n; } Bytes;
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

/* {{{ 302a: WC3's ground, written here and read back (no game data needed).
 * The writer's pieces: the head, then one 7-byte tile point at a time
 * (063-w3e.h has the layout). */
typedef struct { unsigned char *b; size_t n, cap; } W3eOut;
static void wo_raw(W3eOut *o, const void *p, size_t n)
{
    if (o->n + n > o->cap) {
        size_t cap = (o->n + n) * 2;
        unsigned char *b = realloc(o->b, cap);
        if (!b) { fprintf(stderr, "no memory for a WC3 ground file\n"); exit(1); }
        o->b = b; o->cap = cap;
    }
    memcpy(o->b + o->n, p, n);
    o->n += n;
}
static void wo32(W3eOut *o, uint32_t v) { unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; wo_raw(o, b, 4); }
static void wo16(W3eOut *o, uint16_t v) { unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; wo_raw(o, b, 2); }
static void wo8(W3eOut *o, unsigned v) { unsigned char b = (unsigned char)v; wo_raw(o, &b, 1); }
static void wof(W3eOut *o, float f) { uint32_t v; memcpy(&v, &f, 4); wo32(o, v); }
/* ground and cliff: the tileset ids run together ("LdrtLgrs") */
static void w3e_head(W3eOut *o, int version, char code, const char *ground, const char *cliff, int w, int h, float ox, float oy)
{
    o->n = 0;
    wo_raw(o, "W3E!", 4); wo32(o, (uint32_t)version); wo8(o, (unsigned char)code); wo32(o, 1);
    wo32(o, (uint32_t)(strlen(ground) / 4)); wo_raw(o, ground, strlen(ground));
    wo32(o, (uint32_t)(strlen(cliff) / 4)); wo_raw(o, cliff, strlen(cliff));
    wo32(o, (uint32_t)w); wo32(o, (uint32_t)h); wof(o, ox); wof(o, oy);
}
/* marks: byte 4's high bits (0x10 ramp, 0x20 blight, 0x40 water, 0x80 boundary) */
static void w3e_pt(W3eOut *o, int raw_h, unsigned water_word, int tex, int marks, int details, int cvar, int layer, int ctex)
{
    wo16(o, (uint16_t)raw_h); wo16(o, (uint16_t)water_word);
    wo8(o, (unsigned)(tex | marks)); wo8(o, (unsigned)(details | cvar << 5)); wo8(o, (unsigned)(layer | ctex << 4));
}

/* The first ground, 5 x 4 tile points (rows from the south), three ground
 * tilesets (0 and 2 extended), one cliff tileset. Its cells, worked out
 * by hand from 063-w3e.h's rules:
 *   (0,0) bare: its south-west corner is blighted
 *   (3,1) bare: point (4,2) is a cliff level up and no corner is a ramp
 *   (2,2) and (3,2) textured although on two levels: point (3,3) is a ramp
 * Point (1,1) has water over it; (2,1) has the mark but the water is
 * below its ground; (0,3), (4,3) and (2,3) carry the boundary marks. */
static const int W3E_A_TEX[4][5] = { { 0, 0, 1, 1, 2 }, { 0, 1, 1, 2, 2 }, { 0, 0, 0, 2, 2 }, { 1, 1, 0, 0, 0 } };
static const int W3E_A_DETAILS[4][5] = { { 0, 5, 7, 0, 9 }, { 3, 0, 20, 1, 2 }, { 16, 4, 6, 11, 3 }, { 0, 0, 0, 0, 0 } };
static void w3e_fixture_a(W3eOut *o, int version)
{
    w3e_head(o, version, 'L', "LdrtLgrsLrok", "CLdi", 5, 4, -256.0f, -192.0f);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 5; ++x) {
            int raw = 8192 + 4 * (x * 10 + y * 3), marks = 0, layer = 2, ctex = 15, cvar = 0;
            unsigned water = 8192;                                  /* level 0 */
            if (x == 4 && y == 0) raw = -100;                        /* a height stored below 0 */
            if (x == 0 && y == 0) marks |= 0x20;
            if (x == 1 && y == 1) { marks |= 0x40; water = 8192 + 4 * 200; }
            if (x == 2 && y == 1) { marks |= 0x40; water = 8192 + 4 * 10; }
            if (x == 0 && y == 3) marks |= 0x80;
            if (x == 4 && y == 3) water |= 0x4000;
            if (x == 2 && y == 3) water |= 0xC000;
            if ((x == 3 && y == 3) || (x == 4 && y == 3) || (x == 4 && y == 2)) layer = 3;
            if (x == 3 && y == 3) marks |= 0x10;
            if (x == 4 && y == 2) { cvar = 5; ctex = 0; }
            w3e_pt(o, raw, water, W3E_A_TEX[y][x], marks, W3E_A_DETAILS[y][x], cvar, layer, ctex);
        }
}
/* A ground of every bit at random, width x height, from `seed` (the same
 * small generator each run): nground tilesets named "T0aa".. */
static uint32_t w3e_rand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
static void w3e_fixture_random(W3eOut *o, int version, int w, int h, int nground, uint32_t seed)
{
    char ids[16 * 4 + 1] = "";
    for (int k = 0; k < nground; ++k) snprintf(ids + 4 * k, 5, "T%xaa", k);
    w3e_head(o, version, 'Z', ids, "CLdiCLgr", w, h, 1024.5f, -3072.25f);
    for (int k = 0; k < w * h; ++k) {
        unsigned char b[7];
        for (int i = 0; i < 7; ++i) b[i] = (unsigned char)w3e_rand(&seed);
        b[4] = (unsigned char)((b[4] & 0xF0) | (b[4] & 0x0F) % nground);
        b[4] &= (unsigned char)~(w3e_rand(&seed) % 4 ? 0x20 : 0);   /* blight on about a quarter */
        b[6] = (unsigned char)((b[6] & 0xF0) | (2 + w3e_rand(&seed) % 3));   /* levels 2 to 4: cliffs here and there */
        wo_raw(o, b, 7);
    }
}
/* the preparations of the same ground agree to the byte */
static int w3e_preps_same(const W3ePrep *a, const W3ePrep *b)
{
    size_t np = (size_t)a->width * a->height, nc = (size_t)(a->width - 1) * (a->height - 1);
    return a->width == b->width && a->height == b->height && a->used == b->used
        && !memcmp(a->ground_z, b->ground_z, np * sizeof(float)) && !memcmp(a->water_z, b->water_z, np * sizeof(float))
        && !memcmp(a->wet, b->wet, np) && !memcmp(a->cells, b->cells, nc * 8) && !memcmp(a->cell_kind, b->cell_kind, nc);
}
static const uint8_t *w3e_cell(const W3ePrep *p, int i, int j) { return p->cells + ((size_t)j * (p->width - 1) + i) * 8; }
static int w3e_cell_is(const W3ePrep *p, int i, int j, const int *want, int n)
{
    const uint8_t *c = w3e_cell(p, i, j);
    for (int k = 0; k < 8; ++k) if (c[k] != (k < n * 2 ? want[k] : W3E_NONE)) return 0;
    return 1;
}
static double w3e_seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static void check_wc3_ground(void)
{
    W3eOut o = { 0 };
    W3eGround *g = NULL;
    W3ePrep *p = NULL;
    WWhy why;
    char line[700];

    printf("WC3's ground (war3map.w3e, issue 302a)\n");
    for (int version = 11; version <= 12; ++version) {
        w3e_fixture_a(&o, version);
        WErr e = w3e_read(o.b, o.n, &g, &why);
        snprintf(line, sizeof line, "a 5 x 4 ground written here, version %d, is read (%s)", version, e ? why.text : "W_OK");
        check(!e && g->version == version && g->width == 5 && g->height == 4, line);
        if (e) return;
        if (version == 12) break;
        w3e_free(g);
    }
    check(g->tileset_code == 'L' && !strcmp(g->tileset, "lordaeron_summer") && g->custom_tileset == 1,
          "its main tileset: L, lordaeron_summer; its own tileset list");
    check(g->nground == 3 && !memcmp(g->ground_ids, "LdrtLgrsLrok", 12) && g->ncliff == 1 && !memcmp(g->cliff_ids, "CLdi", 4),
          "three ground tilesets and one cliff tileset, by id");
    check(g->offset_x == -256.0f && g->offset_y == -192.0f, "its south-west corner at (-256, -192)");
    const W3ePoint *a = w3e_point(g, 1, 2), *low = w3e_point(g, 4, 0), *cl = w3e_point(g, 4, 2);
    check(a->height_raw == 8192 + 4 * 16 && a->height == 16.0f, "a point's height: (raw - 8192) / 4 = 16");
    check(low->height_raw == -100 && low->height == -2073.0f, "a height stored below 0 (-100) reads as -2073");
    check(cl->layer_height == 3 && cl->cliff_texture == 0 && cl->cliff_variation == 5 && cl->texture_details == 3,
          "byte 6: the cliff level in the low half, the cliff tileset in the high; byte 5: variation low, cliff variation high");
    check(a->cliff_texture == 15 && a->layer_height == 2 && a->ground_texture == 0 && w3e_point(g, 4, 1)->ground_texture == 2,
          "byte 4's low half is the ground tileset; no cliff is 15");
    check(w3e_point(g, 0, 0)->is_blight && w3e_point(g, 3, 3)->is_ramp && w3e_point(g, 1, 1)->has_water
          && w3e_point(g, 0, 3)->is_boundary && !w3e_point(g, 1, 0)->is_blight, "byte 4's marks: blight, ramp, water, boundary");
    check(w3e_point(g, 4, 3)->water_flags == 0x4000 && w3e_point(g, 4, 3)->boundary && w3e_point(g, 2, 3)->water_flags == 0xC000
          && !w3e_point(g, 1, 2)->boundary && w3e_point(g, 2, 3)->water_raw == 8192,
          "the water word's top bits are the other boundary mark, its low 14 bits the level");
    check(w3e_point(g, 5, 0) == NULL && w3e_point(g, 0, -1) == NULL, "outside the grid: no point");
    check(!memcmp(w3e_texture_name(g, 4, 1), "Lrok", 4) && w3e_texture_name(g, 9, 9) == NULL, "a point's tileset by id");
    check(w3e_walkable(g, 1, 0) && !w3e_walkable(g, 1, 1) && !w3e_walkable(g, 0, 3) && !w3e_walkable(g, 4, 3) && !w3e_walkable(g, 5, 0),
          "walkable: not with water, not on either boundary mark, not outside");
    double wx, wy, tx, ty;
    w3e_tile_to_world(g, 2, 3, &wx, &wy);
    w3e_world_to_tile(g, -1.0, -193.0, &tx, &ty);
    check(wx == 0 && wy == 192 && tx == 1 && ty == -1, "tile point (2, 3) is at (0, 192); (-1, -193) is in the cell of (1, -1)");
    check(w3e_ground_z(cl) == 46.0 + 128 && w3e_ground_z(a) == 16.0, "ground z: the height and 128 for each cliff level over 2");
    check(w3e_water_z(w3e_point(g, 1, 1)) == 200 - 89.6 && w3e_is_wet(w3e_point(g, 1, 1)), "water z: the level less 89.6; over the ground, wet");
    check(!w3e_is_wet(w3e_point(g, 2, 1)) && !w3e_is_wet(a), "water below its ground, or no water mark: not wet");
    W3eStats st;
    w3e_stats(g, &st);
    check(st.total == 20 && st.water == 2 && st.blight == 1 && st.ramp == 1 && st.boundary == 3 && st.layers[2] == 17 && st.layers[3] == 3
          && st.min_height == -2073 && st.max_height == 49, "counts: 20 points, 2 with water, 1 blighted, 1 ramp, 3 on the boundary, 3 a level up");

    /* the preparation */
    static const uint8_t EXT[3] = { 1, 0, 1 };
    WErr e = w3e_prep(g, EXT, 3, &p, &why);
    check(!e, e ? why.text : "the ground is prepared");
    if (e) { w3e_free(g); free(o.b); return; }
    check(p->ground_z[2 * 5 + 4] == 174.0f && p->water_z[1 * 5 + 1] == (float)(200 - 89.6) && p->water_z[0] == (float)-1e30
          && p->wet[1 * 5 + 1] == 1 && p->wet[1 * 5 + 2] == 0, "its heights in floats, water -1e30 where there's none");
    check(p->cell_kind[0] == W3E_CELL_BLIGHT && w3e_cell_is(p, 0, 0, NULL, 0), "cell (0, 0), a corner blighted: bare");
    check(p->cell_kind[1 * 4 + 3] == W3E_CELL_CLIFF && w3e_cell_is(p, 3, 1, NULL, 0), "cell (3, 1), a cliff with no ramp: bare");
    check(w3e_cell_is(p, 1, 0, (int[]){ 0, 21, 1, 1 + 8 + 4 }, 2),
          "cell (1, 0): tileset 0 whole (extended, variation 5: subtile 21), then 1 on south-east, north-west, north-east");
    check(w3e_cell_is(p, 2, 1, (int[]){ 0, 0, 1, 2, 2, 1 + 4 }, 3),
          "cell (2, 1): three tilesets, lowest first; extended variation 20 is subtile 0; 1 south-west, 2 south-east and north-east");
    check(w3e_cell_is(p, 3, 0, (int[]){ 1, 15, 2, 13 }, 2), "cell (3, 0): a plain tileset at variation 0 is the whole tile, 15");
    check(w3e_cell_is(p, 0, 2, (int[]){ 0, 15, 1, 8 + 4 }, 2), "cell (0, 2): extended variation 16 is the whole tile, 15");
    check(w3e_cell_is(p, 3, 2, (int[]){ 0, 16 + 11, 2, 2 + 1 }, 2) && w3e_cell_is(p, 2, 2, (int[]){ 0, 16 + 6, 2, 1 }, 2),
          "cells (2, 2) and (3, 2): on two levels but by a ramp, so textured");
    check(w3e_cell_is(p, 0, 1, (int[]){ 0, 16 + 3, 1, 1 }, 2) && p->used == 10, "10 of the 12 cells textured");
    check(w3e_variation(0, 3) == 0 && w3e_variation(1, 15) == 31 && w3e_variation(1, 31) == 0, "subtiles from variations at the edges");
    W3ePrep *plain = NULL;
    if (!w3e_prep(g, NULL, 0, &plain, &why)) {
        check(w3e_cell(plain, 1, 0)[1] == 0 && w3e_cell(plain, 0, 2)[1] == 0 && plain->used == 10, "with no tileset extended: variations 5 and 16 are subtile 0");
        w3e_prep_free(plain);
    } else check(0, why.text);

    /* an editor's stroke: one point changed, only its cells redone */
    W3ePoint *edit = &g->points[1 * 5 + 2];
    edit->ground_texture = 2; edit->layer_height = 4; edit->height = 99;
    W3eArea t;
    e = w3e_prep_area(p, g, 2, 1, 2, 1, &t, &why);
    W3ePrep *fresh = NULL;
    if (!e) e = w3e_prep(g, EXT, 3, &fresh, &why);
    check(!e && t.x0 == 1 && t.y0 == 0 && t.x1 == 2 && t.y1 == 1 && fresh && w3e_preps_same(p, fresh),
          "after one point is raised a cliff level: its four cells redone, and the same bytes as preparing it all again");
    snprintf(line, sizeof line, "with %d textured cells now (the cliff left four bare)", p->used);
    check(p->used == 6 && p->cell_kind[0 * 4 + 1] == W3E_CELL_CLIFF, line);
    w3e_prep_free(fresh); fresh = NULL;
    check(w3e_prep_area(p, g, 7, 0, 9, 3, &t, &why) == W_ERR_REFUSED && t.x1 < t.x0, "an area beside the grid: refused, nothing touched");
    w3e_prep_area(p, g, -5, -5, 0, 0, &t, &why);
    check(t.x0 == 0 && t.y0 == 0 && t.x1 == 0 && t.y1 == 0, "an area over the corner is cut to the grid: one cell");

    /* the same reader on the random ground: every edit's area against a whole preparation */
    w3e_fixture_random(&o, 11, 33, 25, 6, 7);
    W3eGround *r = NULL;
    e = w3e_read(o.b, o.n, &r, &why);
    check(!e && r->nground == 6 && r->ncliff == 2 && !strcmp(r->tileset, "sunken_ruins"), e ? why.text : "a 33 x 25 ground of random points is read");
    if (!e) {
        check(w3e_prep_area(p, r, 0, 0, 1, 1, NULL, &why) == W_ERR_REFUSED, "a preparation refuses a ground of another size");
        static const uint8_t EXT6[6] = { 1, 1, 0, 1, 0, 0 };
        W3ePrep *rp = NULL, *whole = NULL;
        int same = 0, tries = 200;
        uint32_t seed = 99;
        e = w3e_prep(r, EXT6, 6, &rp, &why);
        for (int k = 0; !e && k < tries; ++k) {
            int x0 = (int)(w3e_rand(&seed) % 40) - 4, y0 = (int)(w3e_rand(&seed) % 30) - 4;
            int x1 = x0 + (int)(w3e_rand(&seed) % 6), y1 = y0 + (int)(w3e_rand(&seed) % 6);
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    W3ePoint *q = (W3ePoint *)w3e_point(r, x, y);
                    if (!q) continue;
                    q->ground_texture = (uint8_t)(w3e_rand(&seed) % 6); q->layer_height = (uint8_t)(2 + w3e_rand(&seed) % 3);
                    q->is_ramp = w3e_rand(&seed) % 5 == 0; q->is_blight = w3e_rand(&seed) % 7 == 0;
                    q->texture_details = (uint8_t)(w3e_rand(&seed) % 32); q->has_water = (uint8_t)(w3e_rand(&seed) % 2);
                    q->height = (float)(w3e_rand(&seed) % 400) - 200; q->water_level = (float)(w3e_rand(&seed) % 400) - 200;
                }
            WErr a2 = w3e_prep_area(rp, r, x0, y0, x1, y1, NULL, &why);
            if (a2 == W_ERR_REFUSED) { ++same; continue; }        /* the whole area off the grid: nothing to compare */
            if (a2 || w3e_prep(r, EXT6, 6, &whole, &why)) break;
            same += w3e_preps_same(rp, whole);
            w3e_prep_free(whole); whole = NULL;
        }
        snprintf(line, sizeof line, "%d of %d random strokes: redoing their area alone gives the bytes of preparing it all again", same, tries);
        check(!e && same == tries, line);
        w3e_prep_free(rp);
        w3e_free(r);
    }

    /* the time a stroke costs on a large map */
    w3e_fixture_random(&o, 11, 257, 257, 16, 3);
    if (!w3e_read(o.b, o.n, &r, &why)) {
        W3ePrep *big = NULL;
        double t0 = w3e_seconds();
        e = w3e_prep(r, NULL, 0, &big, &why);
        double t1 = w3e_seconds();
        for (int k = 0; !e && k < 100; ++k) w3e_prep_area(big, r, 100 + k, 100, 104 + k, 104, NULL, &why);
        double t2 = w3e_seconds();
        snprintf(line, sizeof line, "a 257 x 257 ground prepared in %.2f ms; a 5 x 5 stroke redone in %.4f ms", (t1 - t0) * 1e3, (t2 - t1) * 1e3 / 100);
        check(!e && (t2 - t1) / 100 < (t1 - t0), line);
        w3e_prep_free(big);
        w3e_free(r);
    } else check(0, why.text);

    /* files that aren't, each refused with its reason */
    w3e_fixture_a(&o, 11);
    size_t whole_len = o.n;
    static const struct { size_t len; const char *what; } CUT[] = {
        { 3, "three bytes" }, { 10, "cut in its head" }, { 25, "cut in its tileset ids" }, { 50, "cut before its size" },
        { 70, "cut in its points" }, { 0, "nothing" },
    };
    int refused = 0;
    for (size_t k = 0; k < sizeof CUT / sizeof CUT[0]; ++k) {
        size_t n = CUT[k].len ? CUT[k].len : 0;
        if (k == 4) n = whole_len - 1;
        W3eGround *bad = NULL;
        refused += w3e_read(o.b, n, &bad, &why) == W_ERR_CORRUPT && bad == NULL && why.text[0];
    }
    snprintf(line, sizeof line, "%d of 6 grounds cut short refused as corrupt, each with its sentence", refused);
    check(refused == 6, line);
    o.b[0] = 'X';
    W3eGround *bad = NULL;
    check(w3e_read(o.b, o.n, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "W3E!"), "a file not starting W3E!: corrupt");
    o.b[0] = 'W';
    memcpy(o.b + 13, (uint8_t[]){ 0xFF, 0xFF, 0xFF, 0xFF }, 4);    /* the ground tileset count: -1 */
    check(w3e_read(o.b, o.n, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "below 0"), "a tileset count below 0: corrupt (the Lua reads it as none)");
    w3e_head(&o, 11, 'P', "", "", 0, 3, 0, 0);
    check(w3e_read(o.b, o.n, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "0 x 3"), "a ground 0 points wide: corrupt (the Lua reads it as empty)");
    w3e_head(&o, 11, 'P', "Ldrt", "", 1, 1, 0, 0);
    w3e_pt(&o, 8192, 8192, 0, 0, 0, 0, 2, 15);
    W3ePrep *one = NULL;
    check(!w3e_read(o.b, o.n, &bad, &why) && !strcmp(bad->tileset, "unknown_P") && !w3e_prep(bad, NULL, 0, &one, &why) && one->used == 0,
          "a single point, an unknown main tileset: read, named unknown_P, no cells");
    w3e_prep_free(one);
    w3e_free(bad);

    w3e_prep_free(p);
    w3e_free(g);
    free(o.b);
}
/* }}} */

/* {{{ static run: seconds of steps of 0.01 s */
static void run(Mover *m, MoverInput in, double seconds, MoverGround g)
{
    int steps = (int)(seconds * 100 + 0.5);
    for (int i = 0; i < steps; ++i) mover_step(m, &in, 0.01, &(MoverWorld){ g, NULL, NULL, NULL });
}
/* }}} */

/* {{{ 105a: the asset resolver's tests, on folders and an archive made here
 * (no game data needed): a pack replacing an archive's file, an overlay
 * beating the archives, a missing file standing in as a placeholder with
 * its warning, strict mode refusing it, and the folder index and the
 * manifest reader under them (059). Everything is made under
 * DIR/tmp/shared-memory/test-resolver and removed after. */

/* {{{ static the fixtures' writers */
static void t_mkdirs(const char *path)
{
    char p[700];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; *c; ++c)
        if (*c == '/') { *c = '\0'; mkdir(p, 0755); *c = '/'; }
    mkdir(p, 0755);
}
static void t_write(const char *dir, const char *name, const void *bytes, size_t len)
{
    char path[900];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    char *slash = strrchr(path, '/');
    *slash = '\0';
    t_mkdirs(path);
    *slash = '/';
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(bytes, 1, len, f); fclose(f); }
}
static void t_write_text(const char *dir, const char *name, const char *text) { t_write(dir, name, text, strlen(text)); }
static void t_remove_tree(const char *path)
{
    DIR *d = opendir(path);
    if (!d) { unlink(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char sub[900];
        snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        t_remove_tree(sub);
    }
    closedir(d);
    rmdir(path);
}
/* one manifest entry, the fields a test varies and the rest fixed (paths in
 * long brackets, so their backslashes are kept as written) */
static void t_entry(char *out, size_t n, const char *replaces, const char *file, const char *kind, double score)
{
    snprintf(out, n,
             "    { replaces = [[%s]], file = [[%s]], kind = \"%s\",\n"
             "      id = \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\n"
             "      route = \"hand\", lineage = \"independent\",\n"
             "      scores = { { score = %g, date = \"2026-11-01\" } }, seal = false },\n", replaces, file, kind, score);
}
/* }}} */

/* {{{ static t_write_mpq_n: a version 1 archive of whole stored files (no
 * compression, no scrambled files), its two tables scrambled as every
 * archive's are, so 015-mpq.c reads it as it reads the game's */
static uint32_t t_crypt[1280];
static void t_crypt_make(void)
{
    uint32_t seed = 0x00100001;
    for (int i1 = 0; i1 < 256; ++i1)
        for (int i = 0, i2 = i1; i < 5; ++i, i2 += 256) {
            seed = (seed * 125 + 3) % 0x2AAAAB;
            uint32_t hi = (seed & 0xFFFF) << 16;
            seed = (seed * 125 + 3) % 0x2AAAAB;
            t_crypt[i2] = hi | (seed & 0xFFFF);
        }
}
static void t_scramble(uint32_t *w, size_t count, uint32_t key)
{
    uint32_t seed = 0xEEEEEEEE;
    for (size_t i = 0; i < count; ++i) {
        seed += t_crypt[0x400 + (key & 0xFF)];
        uint32_t plain = w[i];
        w[i] = plain ^ (key + seed);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        seed = plain + seed + (seed << 5) + 3;
    }
}
static void t_le32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }
static void t_write_mpq_n(const char *path, const char *const *names, const char *const *contents, const size_t *lens, int n)
{
    enum { NHASH = 16 };
    t_crypt_make();
    size_t data_len = 0;
    for (int i = 0; i < n; ++i) data_len += lens ? lens[i] : strlen(contents[i]);
    size_t hash_at = 32 + data_len, block_at = hash_at + NHASH * 16, total = block_at + (size_t)n * 16;
    unsigned char *s = calloc(1, total);
    memcpy(s, "MPQ\x1a", 4);
    t_le32(s + 4, 32); t_le32(s + 8, (uint32_t)total);
    s[12] = 0; s[14] = 3;                                   /* version 1 (written 0), sectors of 4096 */
    t_le32(s + 16, (uint32_t)hash_at); t_le32(s + 20, (uint32_t)block_at); t_le32(s + 24, NHASH); t_le32(s + 28, (uint32_t)n);
    uint32_t hashes[NHASH * 4], *blocks = calloc((size_t)n * 4, 4);
    memset(hashes, 0xFF, sizeof hashes);
    size_t at = 32;
    for (int i = 0; i < n; ++i) {
        size_t len = lens ? lens[i] : strlen(contents[i]);
        memcpy(s + at, contents[i], len);
        blocks[i * 4 + 0] = (uint32_t)at; blocks[i * 4 + 1] = (uint32_t)len; blocks[i * 4 + 2] = (uint32_t)len;
        blocks[i * 4 + 3] = 0x80000000u | 0x01000000u;      /* there, one piece */
        at += len;
        uint32_t slot = mpq_hash(names[i], 0) % NHASH;
        while (hashes[slot * 4 + 3] != 0xFFFFFFFFu) slot = (slot + 1) % NHASH;
        hashes[slot * 4 + 0] = mpq_hash(names[i], 1); hashes[slot * 4 + 1] = mpq_hash(names[i], 2);
        hashes[slot * 4 + 2] = 0; hashes[slot * 4 + 3] = (uint32_t)i;
    }
    t_scramble(hashes, NHASH * 4, mpq_hash("(hash table)", 3));
    t_scramble(blocks, (size_t)n * 4, mpq_hash("(block table)", 3));
    for (int i = 0; i < NHASH * 4; ++i) t_le32(s + hash_at + (size_t)i * 4, hashes[i]);
    for (int i = 0; i < n * 4; ++i) t_le32(s + block_at + (size_t)i * 4, blocks[i]);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(s, 1, total, f); fclose(f); }
    free(blocks);
    free(s);
}
/* files of text (`lens` NULL above: each content's own length) */
static void t_write_mpq(const char *path, const char *const *names, const char *const *contents, int n) { t_write_mpq_n(path, names, contents, NULL, n); }
/* }}} */

/* {{{ static t_asker: a thread asking the same three assets 100 times (the
 * reading-ahead thread and the main thread share one resolver) */
static void *t_asker(void *arg)
{
    static const char *const ask[] = { "Creature\\Wolf\\Wolf.m2", "Textures\\Ground.blp", "Creature\\Bear\\Bear.m2" };
    long bad = 0;
    for (int i = 0; i < 300; ++i) {
        AssetAnswer a;
        WWhy why;
        if (asset_resolve((Resolver *)arg, ask[i % 3], i % 3 == 1 ? ASSET_TEXTURE : ASSET_MODEL, &a, &why)) ++bad;
        asset_answer_free(&a);
    }
    return (void *)bad;
}
/* }}} */

static void resolver_tests(const char *dir)
{
    char root[400], path[900], line[1000], text[4000], one[600], two[600];
    WWhy why;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-resolver", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the resolver's fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ the folder index (059; issue 104's acceptance, in part) */
    check(!fs_normalise("Creature\\Wolf\\\\Wolf.M2", line, sizeof line) && !strcmp(line, "creature/wolf/wolf.m2")
          && !fs_normalise("/./a//B/./c", path, sizeof path) && !strcmp(path, "a/b/c") && fs_normalise("\\./", line, sizeof line) == 1,
          "names fold to small letters and forward slashes, doubled slashes and ./ parts gone; nothing left is refused");
    snprintf(path, sizeof path, "%s/index", root);
    t_write_text(path, "Creature/Wolf/Wolf.M2", "w");
    t_write_text(path, "readme.txt", "r");
    FsIndex *ix = NULL;
    check(!fs_index_folder(path, &ix, &why) && fs_index_count(ix) == 2, "a folder is indexed: its two files, one two folders down");
    const char *real = fs_index_find(ix, "creature\\wolf\\wolf.m2");
    snprintf(line, sizeof line, "%s/Creature/Wolf/Wolf.M2", path);
    check(real && !strcmp(real, line), "a pack's Creature/Wolf/Wolf.M2 is found by creature\\wolf\\wolf.m2, its real spelling given back");
    check(!fs_index_find(ix, "creature/wolf/wolf.skin") && !fs_index_find(ix, "creature/wolf"), "a name the folder lacks finds nothing, and neither does a folder");
    fs_index_free(ix);
    snprintf(path, sizeof path, "%s/twice", root);
    t_write_text(path, "Art/A.blp", "1");
    t_write_text(path, "art/a.BLP", "2");
    WErr e = fs_index_folder(path, &ix, &why);
    check(e == W_ERR_REFUSED && !ix && strstr(why.text, "Art/A.blp") && strstr(why.text, "art/a.BLP"),
          "two names that differ only by capitals: refused, both named");
    snprintf(path, sizeof path, "%s/no-such-folder", root);
    check(fs_index_folder(path, &ix, &why) == W_ERR_NOT_FOUND, "a folder that isn't there: W_ERR_NOT_FOUND");
    /* }}} */

    /* {{{ the manifest (059) */
    const char *full =
        "-- a pack made by hand for the tests\n"
        "--[[ a block comment,\n over two lines ]]\n"
        "return {\n"
        "    format = 1,\n"
        "    name = 'Wolves, \\\"redrawn\\\"',\n"
        "    entries = {\n"
        "        {\n"
        "            replaces = \"Creature\\\\Wolf\\\\Wolf.m2\", file = \"wolf/wolf.m2\", kind = \"model\",\n"
        "            id = \"3a7bd3e2360a3d29eea436fcfb7e44c735d117c42d1c1835420b6b9942dd4f1b\",\n"
        "            route = \"generate\", lineage = \"independent\",\n"
        "            generation = { prompt = [[a grey wolf,\nlow poly]], seed = \"18446744073709551615\",\n"
        "                           workflow = \"abc\", models = { \"one\", \"two\" } },\n"
        "            scores = { { score = 0.25, date = \"2026-10-01\", by = \"W05a\" }; { score = 6.2e-1, date = \"2026-10-30\" } },\n"
        "            seal = { build = 12340, date = \"2026-11-01\" },\n"
        "            transcript = \"llm-transcripts/nov-01-26.md:120-180\",\n"
        "            wc3 = { scale = 1.5, team_colour = true },\n"
        "        },\n"
        "        { replaces = \"Textures/Ground.blp\", file = \"ground.blp\", kind = \"texture\",\n"
        "          id = \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\n"
        "          route = \"search\", lineage = \"derived\", licence = \"CC0-1.0, someone, https://example.org/ground\" },\n"
        "    },\n"
        "}\n";
    PackManifest *m = NULL;
    e = pack_manifest_parse(full, strlen(full), "test manifest", &m, &why);
    check(!e && m && m->count == 2 && !strcmp(m->name, "Wolves, \"redrawn\""), e ? why.text : "a full manifest reads: its name (escapes undone) and two entries");
    if (m && m->count == 2) {
        const PackEntry *w = &m->entries[0], *g = &m->entries[1];
        check(!strcmp(w->key, "creature/wolf/wolf.m2") && w->kind == ASSET_MODEL && w->route == PACK_ROUTE_GENERATE && w->lineage == PACK_LINEAGE_INDEPENDENT,
              "the wolf: its folded path, kind, route and lineage");
        check(w->has_seed && w->seed == UINT64_MAX && !strcmp(w->prompt, "a grey wolf,\nlow poly") && w->nmodels == 2 && !strcmp(w->models[1], "two"),
              "its generation record: a 64-bit seed held exactly (written as digits), a long-bracket prompt, two model names");
        check(w->nscores == 2 && near(pack_entry_latest(w), 0.62, 1e-12) && !strcmp(w->scores[0].by, "W05a") && !strcmp(w->scores[1].by, ""),
              "two scores, oldest first; the latest is the last (0.62)");
        check(w->sealed && w->seal_build == 12340 && !strcmp(w->seal_date, "2026-11-01") && !strcmp(w->transcript, "llm-transcripts/nov-01-26.md:120-180"),
              "the seal (stock build 12340, checked 2026-11-01) and where the transcript of its making is");
        check(!g->sealed && g->nscores == 0 && pack_entry_latest(g) < 0 && strstr(g->licence, "CC0-1.0") && !g->has_generation,
              "the ground: searched, with its licence; no scores yet, so none latest; no seal");
    }
    pack_manifest_free(m);
    /* each mistake refused, with the code a caller branches on and a sentence naming it */
    struct { const char *text, *finds; WErr code; const char *what; } wrong[] = {
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'hand', linage = 'derived' } } }",
          "linage", W_ERR_REFUSED, "a misspelt field (linage) is refused by name" },
        { "return { format = 2, name = 'x', entries = {} }", "format 2", W_ERR_UNSUPPORTED, "a newer format: W_ERR_UNSUPPORTED" },
        { "return { format = os.exit(), name = 'x', entries = {} }", "line 1", W_ERR_CORRUPT, "a call where data should be: refused as not data, with its line" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'generate', lineage = 'derived' } } }",
          "generation record", W_ERR_REFUSED, "a generated asset with no generation record" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'hand', lineage = 'derived', scores = { { score = 1.5, date = '2026-01-01' } } } } }",
          "1.5", W_ERR_REFUSED, "a score past 1" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'hand', lineage = 'derived', scores = { { score = 0.5, date = '2026-02-01' }, { score = 0.6, date = '2026-01-01' } } } } }",
          "oldest first", W_ERR_REFUSED, "scores out of date order" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'hand', lineage = 'derived', seal = true } } }",
          "W05c", W_ERR_REFUSED, "a seal of plain true (it must name the build and date it was checked)" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = '%s', route = 'generate', lineage = 'derived', generation = { seed = 12 } } } }",
          "string of digits", W_ERR_REFUSED, "a seed written as a number (LuaJIT would round a large one)" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'A\\\\b', file = 'a', kind = 'model', id = '%s', route = 'hand', lineage = 'derived' },\n"
          "{ replaces = 'a/B', file = 'b', kind = 'model', id = '%s', route = 'hand', lineage = 'derived' } } }",
          "both replace", W_ERR_REFUSED, "two entries replacing one path (spelt differently)" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'mesh', id = '%s', route = 'hand', lineage = 'derived' } } }",
          "not one of", W_ERR_REFUSED, "a kind that isn't one" },
        { "return { format = 1, name = 'x', entries = { { replaces = 'a', file = 'a', kind = 'model', id = 'ABC', route = 'hand', lineage = 'derived' } } }",
          "SHA-256", W_ERR_REFUSED, "an id that isn't a SHA-256" },
        { "return { format = 1, name = 'x', name = 'y', entries = {} }", "twice", W_ERR_CORRUPT, "a key given twice" },
        { "return { format = 1, name = \"no end, entries = {} }", "doesn't close", W_ERR_CORRUPT, "a string that never closes" },
    };
    const char *hex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    for (size_t i = 0; i < sizeof wrong / sizeof wrong[0]; ++i) {
        snprintf(text, sizeof text, wrong[i].text, hex, hex);
        why.text[0] = '\0';
        e = pack_manifest_parse(text, strlen(text), "test manifest", &m, &why);
        snprintf(line, sizeof line, "%s: %s (%s)", wrong[i].what, w_err_name(e), why.text);
        check(e == wrong[i].code && !m && strstr(why.text, wrong[i].finds), line);
    }
    check(!pack_manifest_parse("return { format = 1, name = 'empty', entries = {}, }", 52, "t", &m, &why) && m && m->count == 0,
          "a pack with nothing in it yet (entries = {}, a comma after the last item) reads");
    pack_manifest_free(m);
    /* }}} */

    /* {{{ the fixtures: an archive, two packs, an overlay */
    char data[520], pack1[520], pack2[520], overlay[520], broken[520];
    snprintf(data, sizeof data, "%s/Data", root);
    t_mkdirs(data);
    snprintf(path, sizeof path, "%s/patch.MPQ", data);
    const char *const names[] = { "Creature\\Wolf\\Wolf.m2", "Textures\\Ground.blp", "Sound\\Howl.wav" };
    const char *const bytes[] = { "the archive's wolf", "the archive's ground", "the archive's howl" };
    t_write_mpq(path, names, bytes, 3);
    MpqChain chain;
    e = mpq_chain_open(&chain, data, "enUS", &why);
    check(!e && chain.count == 1, e ? why.text : "a made-up archive opens as a chain of one");
    if (e) { t_remove_tree(root); return; }

    snprintf(pack1, sizeof pack1, "%s/packs/wolves", root);
    t_entry(one, sizeof one, "Creature\\Wolf\\Wolf.m2", "Wolf/WOLF.m2", "model", 0.62);
    t_entry(two, sizeof two, "Creature\\Cat\\Cat.m2", "cat.m2", "model", 0.3);
    snprintf(text, sizeof text, "return {\n  format = 1, name = \"Wolves\",\n  entries = {\n%s%s  },\n}\n", one, two);
    t_write_text(pack1, "manifest.lua", text);
    t_write_text(pack1, "wolf/Wolf.m2", "the pack's wolf");
    t_write_text(pack1, "cat.m2", "the pack's cat");
    t_write_text(pack1, "notes.txt", "a file no entry names");
    snprintf(pack2, sizeof pack2, "%s/packs/second", root);
    t_entry(one, sizeof one, "creature/wolf/wolf.m2", "w.m2", "model", 0.9);
    t_entry(two, sizeof two, "Sound\\Howl.wav", "howl.wav", "model", 0.9);    /* listed as a model: asked for as a sound, refused */
    snprintf(text, sizeof text, "return { format = 1, name = \"Second\", entries = {\n%s%s} }\n", one, two);
    t_write_text(pack2, "MANIFEST.LUA", text);
    t_write_text(pack2, "w.m2", "the second pack's wolf");
    t_write_text(pack2, "howl.wav", "the second pack's howl");
    snprintf(overlay, sizeof overlay, "%s/overlays/map", root);
    t_write_text(overlay, "TEXTURES/ground.BLP", "the overlay's ground");
    /* }}} */

    /* {{{ the lookup, its order and its counts */
    ResolverPlace place = { .npacks = 2, .noverlays = 1, .chain = &chain, .distinctness = 0.5, .trace = 1 };
    snprintf(place.packs[0], sizeof place.packs[0], "%s", pack1);
    snprintf(place.packs[1], sizeof place.packs[1], "%s", pack2);
    snprintf(place.overlays[0], sizeof place.overlays[0], "%s", overlay);
    char *log = NULL;
    size_t log_len = 0;
    place.log = open_memstream(&log, &log_len);
    Resolver *r = NULL;
    e = resolver_open(&place, &r, &why);
    check(!e && r, e ? why.text : "the resolver opens two packs, an overlay and the archive");
    if (e) { fclose(place.log); free(log); mpq_chain_close(&chain); t_remove_tree(root); return; }
    AssetAnswer a;
    e = asset_resolve(r, "CREATURE/wolf/wolf.M2", ASSET_MODEL, &a, &why);
    check(!e && a.source == ASSET_FROM_PACK && a.len == 15 && !memcmp(a.data, "the pack's wolf", 15) && !strcmp(a.from, pack1),
          "a pack file replaces the archive's file of the same logical path, in any spelling (105a's acceptance)");
    check(a.entry && near(pack_entry_latest(a.entry), 0.62, 1e-12) && !a.entry->sealed, "and the answer carries its manifest entry: its score and seal");
    asset_answer_free(&a);
    e = asset_resolve(r, "Creature\\Wolf\\Wolf.m2", ASSET_MODEL, &a, &why);
    check(!e && a.source == ASSET_FROM_PACK && !memcmp(a.data, "the pack's wolf", 15), "the first pack listed outranks the second, which replaces the wolf too");
    asset_answer_free(&a);
    e = asset_resolve(r, "Textures\\Ground.blp", ASSET_TEXTURE, &a, &why);
    check(!e && a.source == ASSET_FROM_OVERLAY && a.len == 20 && !memcmp(a.data, "the overlay's ground", 20) && !strcmp(a.from, overlay),
          "an overlay's file beats the archive's");
    asset_answer_free(&a);
    e = asset_resolve(r, "Sound\\Howl.wav", ASSET_SOUND, &a, &why);
    check(e == W_ERR_REFUSED && !a.data && strstr(why.text, "lists it as a model"),
          "a pack entry of another kind than asked for: refused, naming both kinds");
    e = asset_resolve(r, "Creature\\Cat\\Cat.m2", ASSET_MODEL, &a, &why);
    check(!e && a.source == ASSET_FROM_PACK && !memcmp(a.data, "the pack's cat", 14), "the cat, from the pack (score 0.3: below the threshold)");
    asset_answer_free(&a);
    e = asset_resolve(r, "Creature\\Bear\\Bear.m2", ASSET_MODEL, &a, &why);
    check(!e && a.source == ASSET_FROM_PLACEHOLDER && !strcmp(a.from, "built in"), "a model in no pack, folder or archive: a placeholder stands in");
    AssetAnswer skin;
    e = asset_resolve(r, "Creature\\Bear\\Bear00.skin", ASSET_MODEL, &skin, &why);
    M2Model box;
    WErr me = (e || !a.data) ? W_ERR_NOT_FOUND : m2_read(a.data, a.len, skin.data, skin.len, &box, &why);
    check(!me && box.nvertices == 8 && box.nindices == 36 && box.npieces == 1 && box.nunits == 1,
          me ? why.text : "the placeholder model and its .skin are an M2 the model reader takes: a box, 8 corners, 12 triangles");
    if (!me) {
        float lo = 1e9f, hi = -1e9f;
        for (uint32_t i = 0; i < box.nvertices; ++i) { if (box.vertices[i].z < lo) lo = box.vertices[i].z; if (box.vertices[i].z > hi) hi = box.vertices[i].z; }
        check(lo == 0 && hi == 1, "standing on the ground, a yard high");
        m2_free(&box);
    }
    asset_answer_free(&a);
    asset_answer_free(&skin);
    e = asset_resolve(r, "Textures\\Missing.blp", ASSET_TEXTURE, &a, &why);
    BlpImage img = { 0 };
    WErr be = (e || !a.data) ? W_ERR_NOT_FOUND : blp_decode(a.data, a.len, &img, &why);
    check(!be && img.width == 8 && img.height == 8 && img.rgba[0] == 255 && img.rgba[1] == 0 && img.rgba[2] == 255
          && img.rgba[2 * 4] == 0 && img.rgba[2 * 4 + 2] == 0 && img.rgba[3] == 255,
          be ? why.text : "the placeholder picture is a BLP2 the picture reader takes: 8 x 8, magenta and black squares");
    if (!be) blp_free(&img);
    asset_answer_free(&a);
    e = asset_resolve(r, "Sound\\Missing.wav", ASSET_SOUND, &a, &why);
    check(!e && a.len > 44 && !memcmp(a.data, "RIFF", 4) && !memcmp(a.data + 8, "WAVE", 4), "the placeholder sound is a WAV (the beep)");
    asset_answer_free(&a);
    e = asset_resolve(r, "Fonts\\Missing.ttf", ASSET_FONT, &a, &why);
    check(!e && a.source == ASSET_FROM_PLACEHOLDER && a.len == 0, "a font's placeholder is empty: the client's built-in font");
    asset_answer_free(&a);
    e = asset_resolve(r, "Creature\\Bear\\Bear.m2", ASSET_MODEL, &a, &why);
    asset_answer_free(&a);
    check(asset_resolve(r, "", ASSET_MODEL, &a, &why) == W_ERR_REFUSED && asset_resolve(r, "x", (AssetKind)99, &a, &why) == W_ERR_REFUSED,
          "an empty name, or a kind outside the list: refused");
    AssetCounts c;
    asset_counts(r, &c);
    check(c.by[ASSET_MODEL][ASSET_FROM_PACK] == 2 && c.distinct[ASSET_MODEL] == 1 && c.by[ASSET_MODEL][ASSET_FROM_PLACEHOLDER] == 2
          && c.by[ASSET_TEXTURE][ASSET_FROM_OVERLAY] == 1 && c.by[ASSET_TEXTURE][ASSET_FROM_PLACEHOLDER] == 1
          && c.by[ASSET_SOUND][ASSET_FROM_PACK] == 0 && c.by[ASSET_SOUND][ASSET_FROM_PLACEHOLDER] == 1 && c.by[ASSET_FONT][ASSET_FROM_PLACEHOLDER] == 1,
          "counted once per asset, by kind and place: models 2 from the pack (1 past the threshold) and 2 placeholders; asked again, not again; refused, not at all");
    check(c.placeholder_answers == 6, "placeholders counted every time they stood in (6: the bear twice)");
    check(resolver_exit_status(r) == 0, "strict mode off: placeholders don't change the exit status");
    fflush(place.log);
    check(log && strstr(log, "pack \"Wolves\"") && strstr(log, "1 file with no entry") && strstr(log, "notes.txt"),
          "a pack file no entry names is said at the start, by name (it is never served)");
    check(log && strstr(log, "WARNING: the model Creature\\Bear\\Bear.m2 is in none of 2 packs, 0 server folders, 1 overlay or the archives; a box stands in"),
          "each placeholder is a warning naming the asset and where it was looked for");
    check(log && strstr(log, "assets: texture Textures\\Ground.blp <- overlay (") && strstr(log, "<- pack \"Wolves\"") && strstr(log, "score 0.62, replaced"),
          "the trace names the winner of each request (and a pack asset's score)");
    char *table = NULL;
    size_t table_len = 0;
    FILE *tf = open_memstream(&table, &table_len);
    resolver_report(r, tf);
    fclose(tf);
    check(table && strstr(table, "model") && strstr(table, "2 (1)") && strstr(table, "placeholders stood in 6 times; strict mode off"),
          "the counts table: a row a kind, the pack column with how many are past the threshold");
    if (table) fputs(table, stdout);
    free(table);
    pthread_t askers[4];
    long bad = 0;
    for (int i = 0; i < 4; ++i) pthread_create(&askers[i], NULL, t_asker, r);
    for (int i = 0; i < 4; ++i) { void *got; pthread_join(askers[i], &got); bad += (long)got; }
    AssetCounts after;
    asset_counts(r, &after);
    c.placeholder_answers += 4 * 100;
    check(bad == 0 && !memcmp(&after, &c, sizeof c), "four threads asking the same assets at once: every answer given, each asset still counted once, every placeholder counted");
    resolver_free(r);
    /* the archive answers what no folder has: a resolver with no folders at all */
    place.npacks = 0; place.noverlays = 0; place.trace = 0;
    e = resolver_open(&place, &r, &why);
    if (!e) {
        e = asset_resolve(r, "Sound\\Howl.wav", ASSET_SOUND, &a, &why);
        check(!e && a.source == ASSET_FROM_BLIZZARD && a.len == 18 && !memcmp(a.data, "the archive's howl", 18) && a.from && strstr(a.from, "patch.MPQ"),
              "with no folders, the archive's file answers, naming the archive");
        asset_answer_free(&a);
        resolver_free(r);
    } else check(0, why.text);
    /* }}} */

    /* {{{ strict mode */
    place.strict = 1;
    e = resolver_open(&place, &r, &why);
    if (!e) {
        e = asset_resolve(r, "Creature\\Bear\\Bear.m2", ASSET_MODEL, &a, &why);
        check(e == W_ERR_NOT_FOUND && !a.data && strstr(why.text, "strict mode"), "strict mode: a placeholder is an error, W_ERR_NOT_FOUND, no bytes");
        check(resolver_exit_status(r) == 1, "and the program's exit status for its assets turns to 1 (105a's acceptance: a non-zero exit)");
        resolver_free(r);
    } else check(0, why.text);
    place.strict = 0;
    /* }}} */

    /* {{{ broken packs stop the start, by name */
    place.npacks = 1;
    snprintf(place.packs[0], sizeof place.packs[0], "%s", overlay);
    e = resolver_open(&place, &r, &why);
    check(e == W_ERR_NOT_FOUND && !r && strstr(why.text, "no manifest.lua"), "a pack folder with no manifest: W_ERR_NOT_FOUND, said so");
    snprintf(broken, sizeof broken, "%s/packs/broken", root);
    t_entry(one, sizeof one, "a.blp", "not-there.blp", "texture", 0.5);
    snprintf(text, sizeof text, "return { format = 1, name = 'broken', entries = {\n%s} }\n", one);
    t_write_text(broken, "manifest.lua", text);
    snprintf(place.packs[0], sizeof place.packs[0], "%s", broken);
    e = resolver_open(&place, &r, &why);
    check(e == W_ERR_NOT_FOUND && strstr(why.text, "not-there.blp") && strstr(why.text, "line 2"), "an entry whose file isn't in its pack: refused at the start, with its line");
    snprintf(place.packs[0], sizeof place.packs[0], "%s/packs/none", root);
    check(resolver_open(&place, &r, &why) == W_ERR_NOT_FOUND && strstr(why.text, "not there"), "a pack folder that isn't there");
    /* }}} */

    /* {{{ the settings */
    Settings *rs = settings_new();
    check(!strcmp(settings_text(rs, "assetPacks"), "") && settings_num(rs, "assetsStrict") == 0 && near(settings_num(rs, "assetsDistinctness"), 0.5, 1e-12),
          "by default: no packs, strict mode off, the threshold 0.5");
    check(settings_set(rs, "assetPacks", "../elsewhere", &why) == W_ERR_REFUSED && settings_set(rs, "assetPacks", "a/b", &why) == W_ERR_REFUSED
          && settings_set(rs, "assetPacks", "a,,b", &why) == W_ERR_REFUSED && settings_set(rs, "assetsDistinctness", "1.5", &why) == W_ERR_REFUSED,
          "pack names with a slash, a .., or an empty name between commas are refused, and so is a threshold past 1");
    settings_set(rs, "assetPacks", "wolves,second", &why);
    settings_set(rs, "assetOverlays", "map", &why);
    settings_set(rs, "assetsStrict", "1", &why);
    ResolverPlace sp;
    e = resolver_place_from_settings(rs, root, &chain, &sp, &why);
    snprintf(path, sizeof path, "%s/packs/second", root);
    check(!e && sp.npacks == 2 && !strcmp(sp.packs[0], pack1) && !strcmp(sp.packs[1], path) && sp.noverlays == 1 && !strcmp(sp.overlays[0], overlay)
          && sp.strict == 1 && near(sp.distinctness, 0.5, 1e-12) && sp.chain == &chain,
          "the place from the settings: the named folders under assets/packs and assets/overlays, in order");
    settings_free(rs);
    /* }}} */

    fclose(place.log);
    free(log);
    mpq_chain_close(&chain);
    t_remove_tree(root);
}
/* }}} */

/* {{{ 1109: the test map's launch and its goodbye receipt, on folders made
 * here (no game data needed): the options read and refused, the password
 * rule, the map's folder first among the resolver's overlays, and the
 * receipt written whole, read back with LuaJIT as the editor reads it, and
 * refused where it can't be written. Everything is made under
 * DIR/tmp/shared-memory/test-test-map and removed after. */

/* {{{ static the test map's helpers */
/* the parse of a NULL-ended argument list (argv[0] the program's name) */
static const ClientOption T_OPTIONS[] = {
    { "--maps", 1 }, { "--client", 1 }, { "--settings", 1 }, { "--front", 0 }, { "--goto", 2 }, { "--map", 3 }, { NULL, 0 },
};
static WErr t_parse(const char *password, TestMap **tm, WWhy *why, ...)
{
    char *argv[40] = { "w-client" };
    int argc = 1;
    va_list ap;
    va_start(ap, why);
    for (char *a; argc < 39 && (a = va_arg(ap, char *)); ) argv[argc++] = a;
    va_end(ap);
    why->code = W_OK; why->text[0] = '\0';
    return test_map_parse(argc, argv, T_OPTIONS, password, tm, why);
}
/* the bytes of `s` as small hex digits (how LuaJIT's reading is compared) */
static void t_hex(const char *s, char *out, size_t n)
{
    size_t k = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && k + 3 < n; ++p) k += (size_t)snprintf(out + k, n - k, "%02x", *p);
    out[k] = '\0';
}
/* the receipt at `path` read by LuaJIT as the editor reads it (loadfile, an
 * empty environment), printed as format|map in hex|started|ended|seconds|
 * frames|reason|why; "" when LuaJIT refused it or isn't there */
static void t_luajit_read(const char *path, char *out, size_t n)
{
    char cmd[2000];
    snprintf(cmd, sizeof cmd,
             "/usr/bin/luajit -e 'local c, e = loadfile([==[%s]==], \"t\", {}); if not c then io.write(\"refused: \", e) return end; "
             "local r = c(); local h = (r.map or \"\"):gsub(\".\", function(b) return string.format(\"%%02x\", b:byte()) end); "
             "io.write(tostring(r.format), \"|\", h, \"|\", tostring(r.started), \"|\", tostring(r.ended), \"|\", "
             "type(r.seconds) == \"number\" and string.format(\"%%.3f\", r.seconds) or \"none\", \"|\", tostring(r.frames), \"|\", "
             "tostring(r.reason), \"|\", tostring(r.why))' 2>&1", path);
    out[0] = '\0';
    fflush(stdout);                                       /* so the child doesn't inherit lines not yet written */
    FILE *p = popen(cmd, "r");
    if (!p) return;
    size_t got = fread(out, 1, n - 1, p);
    out[got] = '\0';
    pclose(p);
}
static int t_is_utc(const char *s)
{
    static const char shape[] = "dddd-dd-ddTdd:dd:ddZ";
    if (strlen(s) != 20) return 0;
    for (int i = 0; i < 20; ++i) if (shape[i] == 'd' ? !(s[i] >= '0' && s[i] <= '9') : s[i] != shape[i]) return 0;
    return 1;
}
/* files in `dir` whose names start with `start` */
static int t_count_starting(const char *dir, const char *start)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) if (!strncmp(e->d_name, start, strlen(start))) ++n;
    closedir(d);
    return n;
}
static char *t_read_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    static char buf[8192];
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}
/* }}} */

static void test_map_tests(const char *dir)
{
    char root[400], path[900], line[1200], got[2000], hex[400];
    WWhy why;
    WErr e;
    TestMap *tm = NULL;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-test-map", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the test map's fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ the options: good */
    e = t_parse("pw", &tm, &why, "--maps", "M", "--test-map", "/maps/daow-build", "--client", "C", "--receipt", "/tmp/r.lua",
                "--camera", "12.5,-300", "--realm", "10.0.0.2:3725", "--account", "TESTER", "--front", "--goto", "-1", "2", NULL);
    double cx = 0, cy = 0;
    check(!e && tm && !strcmp(test_map_folder(tm), "/maps/daow-build") && !strcmp(test_map_receipt(tm), "/tmp/r.lua")
          && test_map_camera(tm, &cx, &cy) && cx == 12.5 && cy == -300 && !strcmp(test_map_realm_host(tm), "10.0.0.2")
          && test_map_realm_port(tm) == 3725 && !strcmp(test_map_account(tm), "TESTER") && !strcmp(test_map_password(tm), "pw"),
          e ? why.text : "a test-map run among the client's own options: every value read (a negative --goto value is a value, not an option)");
    test_map_free(tm);
    e = t_parse("pw", &tm, &why, "--test-map", "m", "--receipt", "r", NULL);
    check(!e && tm && !test_map_camera(tm, &cx, &cy) && !strcmp(test_map_realm_host(tm), "127.0.0.1") && test_map_realm_port(tm) == 3724
          && !strcmp(test_map_account(tm), ""), "only --test-map and --receipt: the map's own start, the local server at 127.0.0.1:3724, no account");
    test_map_free(tm);
    e = t_parse(NULL, &tm, &why, "--maps", "M", "--client", "C", "--map", "0", "-8913", "554", NULL);
    check(!e && !tm, "no --test-map: not a test-map run, no handle, and no password needed");
    /* }}} */

    /* {{{ the options: refused */
    static const struct { const char *args[8]; WErr code; int handle; const char *finds; const char *what; } wrong[] = {
        { { "--receipt", "r", "--test-map", NULL }, W_ERR_REFUSED, 1, "--test-map needs a value", "--test-map last, with nothing after it" },
        { { "--test-map", "--receipt", "r", NULL }, W_ERR_REFUSED, 1, "--test-map needs a value", "--test-map followed by another option" },
        { { "--test-map", "m", "--receipt", NULL }, W_ERR_REFUSED, 0, "--receipt needs a value", "--receipt with no file: refused, and no receipt to write" },
        { { "--test-map", "m", "--receipt", "r", "--goto", "1", NULL }, W_ERR_REFUSED, 1, "--goto needs 2 values after it, and has 1", "one of the client's own options short of its values" },
        { { "--test-map", "m", "--receipt", "r", "--account", "--realm", "h:1", NULL }, W_ERR_REFUSED, 1, "--account needs a value", "--account followed by another option" },
        { { "--test-map", "m", "--receipt", "r", "--frobnicate", NULL }, W_ERR_REFUSED, 1, "the option --frobnicate isn't one this client takes", "an option the client doesn't know, in a test-map run: refused, the receipt still written" },
        { { "--maps", "M", "--frobnicate", NULL }, W_ERR_REFUSED, 0, "the option --frobnicate isn't one this client takes", "an option the client doesn't know, in an ordinary run" },
        { { "--maps", "M", "stray", NULL }, W_ERR_REFUSED, 0, "the argument \"stray\" follows no option", "an argument no option takes" },
        { { "--test-map", "a", "--receipt", "r", "--test-map", "b", NULL }, W_ERR_REFUSED, 1, "--test-map was given twice", "--test-map twice" },
        { { "--receipt", "r", NULL }, W_ERR_REFUSED, 0, "--receipt is for a --test-map run", "--receipt with no --test-map" },
        { { "--maps", "M", "--camera", "1,2", NULL }, W_ERR_REFUSED, 0, "--camera is for a --test-map run", "--camera with no --test-map" },
        { { "--test-map", "m", NULL }, W_ERR_NOT_FOUND, 0, "needs --receipt FILE", "--test-map with no --receipt" },
    };
    for (unsigned i = 0; i < sizeof wrong / sizeof wrong[0]; ++i) {
        const char *const *a = wrong[i].args;
        e = t_parse("pw", &tm, &why, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        snprintf(line, sizeof line, "%s: %s (%s)", wrong[i].what, w_err_name(e), why.text);
        check(e == wrong[i].code && (tm != NULL) == wrong[i].handle && strstr(why.text, wrong[i].finds), line);
        if (i == 1) check(tm && !strcmp(test_map_folder(tm), "") && !strcmp(test_map_receipt(tm), "r"), "and the receipt is still known, with no map folder");
        if (i == 8) check(tm && !strcmp(test_map_folder(tm), "a"), "and the first one stands");
        test_map_free(tm);
    }
    static const char *const cameras[] = { "12", "12,", ",5", "a,b", "1,2,3", "1;2", "nan,1", "1e999,0", "1, 2", "" };
    int all = 1;
    for (unsigned i = 0; i < sizeof cameras / sizeof cameras[0]; ++i) {
        e = t_parse("pw", &tm, &why, "--test-map", "m", "--receipt", "r", "--camera", cameras[i], NULL);
        if (!(e == W_ERR_REFUSED && tm && strstr(why.text, "--camera takes") && !test_map_camera(tm, &cx, &cy))) { all = 0; printf("        camera \"%s\": %s\n", cameras[i], why.text); }
        test_map_free(tm);
    }
    check(all, "a camera that isn't two finite numbers with a comma between is refused, each of 10 ways (12; 12,; ,5; a,b; 1,2,3; 1;2; nan,1; 1e999,0; a space; nothing)");
    e = t_parse("pw", &tm, &why, "--test-map", "m", "--receipt", "r", "--camera", "-0.25,1e3", NULL);
    check(!e && test_map_camera(tm, &cx, &cy) && cx == -0.25 && cy == 1000, "a camera of -0.25,1e3: read as written");
    test_map_free(tm);
    static const char *const realms[] = { "localhost", "localhost:", ":3724", "h:0", "h:65536", "h:12x", "my host:3724" };
    all = 1;
    for (unsigned i = 0; i < sizeof realms / sizeof realms[0]; ++i) {
        e = t_parse("pw", &tm, &why, "--test-map", "m", "--receipt", "r", "--realm", realms[i], NULL);
        if (!(e == W_ERR_REFUSED && tm && strstr(why.text, "--realm takes"))) { all = 0; printf("        realm \"%s\": %s\n", realms[i], why.text); }
        test_map_free(tm);
    }
    check(all, "a realm that isn't host:port with a port from 1 to 65535 is refused, each of 7 ways");
    e = t_parse("pw", &tm, &why, "--test-map", "m", "--receipt", "r", "--account", "two words", NULL);
    check(e == W_ERR_REFUSED && strstr(why.text, "--account takes"), "an account name with a space is refused");
    test_map_free(tm);
    /* }}} */

    /* {{{ the password: from W_CLIENT_PASSWORD only */
    e = t_parse(NULL, &tm, &why, "--test-map", "m", "--receipt", "r", NULL);
    check(e == W_ERR_NOT_FOUND && tm && strstr(why.text, "W_CLIENT_PASSWORD") && strstr(why.text, "not set") && strstr(why.text, "never taken on the command line"),
          "a test-map run with no W_CLIENT_PASSWORD stops, saying where the password is read from, and the receipt can say why");
    test_map_free(tm);
    e = t_parse("", &tm, &why, "--test-map", "m", "--receipt", "r", NULL);
    check(e == W_ERR_NOT_FOUND && tm && strstr(why.text, "empty"), "an empty one is missing too");
    test_map_free(tm);
    e = t_parse("sekrit", &tm, &why, "--test-map", "m", "--receipt", "r", "--password", "sekrit", NULL);
    check(e == W_ERR_REFUSED && strstr(why.text, "--password isn't one this client takes") && !strstr(why.text, "sekrit"),
          "there is no --password option: refused by name, its value not repeated in the sentence");
    test_map_free(tm);
    e = t_parse("sekrit", &tm, &why, "--test-map", "m", "--receipt", "r", "--camera", "x", NULL);
    check(e && !strstr(why.text, "sekrit"), "a refusal never names the password");
    test_map_free(tm);
    /* }}} */

    /* {{{ the map first among the resolver's overlays */
    char assets[600], base[700], build[600], pack[700];
    snprintf(assets, sizeof assets, "%s/assets", root);
    snprintf(base, sizeof base, "%s/overlays/base", assets);
    snprintf(pack, sizeof pack, "%s/packs/p", assets);
    snprintf(build, sizeof build, "%s/daow-build", root);
    t_write_text(base, "Textures/Ground.blp", "the settings' overlay's ground");
    t_write_text(base, "Textures/Only.blp", "only in the settings' overlay");
    t_write_text(build, "TEXTURES/ground.BLP", "the test map's ground");
    t_write_text(build, "Textures/Pack.blp", "the test map's file the pack replaces");
    char entry[700], manifest[1200];
    t_entry(entry, sizeof entry, "Textures\\Pack.blp", "pack.blp", "texture", 0.8);
    snprintf(manifest, sizeof manifest, "return { format = 1, name = 'p', entries = {\n%s} }\n", entry);
    t_write_text(pack, "manifest.lua", manifest);
    t_write_text(pack, "pack.blp", "the pack's file");
    Settings *ts = settings_new();
    settings_set(ts, "assetOverlays", "base", &why);
    settings_set(ts, "assetPacks", "p", &why);
    e = t_parse("pw", &tm, &why, "--test-map", build, "--receipt", "r", NULL);
    ResolverPlace place;
    e = e ? e : test_map_place(tm, ts, assets, NULL, &place, &why);
    check(!e && place.noverlays == 2 && !strcmp(place.overlays[0], build) && !strcmp(place.overlays[1], base) && place.npacks == 1,
          e ? why.text : "the place: the map's folder first among the overlays, the settings' overlay after it, the pack kept");
    Resolver *r = NULL;
    char *log = NULL;
    size_t log_len = 0;
    place.log = open_memstream(&log, &log_len);
    e = e ? e : resolver_open(&place, &r, &why);
    check(!e, e ? why.text : "the resolver opens on it");
    if (!e) {
        AssetAnswer a;
        e = asset_resolve(r, "textures\\ground.blp", ASSET_TEXTURE, &a, &why);
        check(!e && a.source == ASSET_FROM_OVERLAY && !strcmp(a.from, build) && a.len == 21 && !memcmp(a.data, "the test map's ground", 21),
              "a file in both overlays: the test map's answers, from its folder");
        asset_answer_free(&a);
        e = asset_resolve(r, "Textures/Only.blp", ASSET_TEXTURE, &a, &why);
        check(!e && a.source == ASSET_FROM_OVERLAY && !strcmp(a.from, base), "a file only in the settings' overlay: still found, behind the map");
        asset_answer_free(&a);
        e = asset_resolve(r, "Textures\\Pack.blp", ASSET_TEXTURE, &a, &why);
        check(!e && a.source == ASSET_FROM_PACK && a.len == 15, "a pack's file still comes before the map's (105a's order: packs, the server's, overlays, the archives)");
        asset_answer_free(&a);
    }
    resolver_free(r);
    fclose(place.log);
    free(log);
    snprintf(path, sizeof path, "%s/Config.wtf", root);
    char *saved = settings_save(ts, path, &why) ? NULL : t_read_text(path);
    check(!strcmp(settings_text(ts, "assetOverlays"), "base") && saved && !strstr(saved, "daow-build"),
          "the settings are unchanged: assetOverlays is still base, and a saved settings file doesn't name the map");
    e = test_map_place(tm, ts, NULL, NULL, &place, &why);
    check(e == W_ERR_REFUSED && strstr(why.text, "no --assets folder"), "settings naming folders, and no assets folder to find them under: refused");
    Settings *plain = settings_new();
    e = test_map_place(tm, plain, NULL, NULL, &place, &why);
    check(!e && place.noverlays == 1 && place.npacks == 0 && !strcmp(place.overlays[0], build), "no folders in the settings and no assets folder: the map alone");
    settings_set(plain, "assetOverlays", "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p", &why);
    e = test_map_place(tm, plain, assets, NULL, &place, &why);
    check(e == W_ERR_FULL && strstr(why.text, "no room"), "16 overlays in the settings already: no room in front of them, said so");
    settings_free(plain);
    settings_free(ts);
    test_map_free(tm);
    e = t_parse("pw", &tm, &why, "--test-map", "--receipt", "r", NULL);
    Settings *none = settings_new();
    check(tm && test_map_place(tm, none, NULL, NULL, &place, &why) == W_ERR_REFUSED && strstr(why.text, "no folder"), "a test map with no folder has no place");
    settings_free(none);
    test_map_free(tm);
    /* }}} */

    /* {{{ the receipt */
    char goodbye_utc_text[21];
    goodbye_utc(1793824267, goodbye_utc_text);
    check(!strcmp(goodbye_utc_text, "2026-11-04T20:31:07Z"), "times are written as 2026-11-04T20:31:07Z, in UTC");
    check(!strcmp(goodbye_reason_name(GOODBYE_CLOSED), "closed") && !strcmp(goodbye_reason_name(GOODBYE_LOST_CONNECTION), "lost-connection")
          && !strcmp(goodbye_reason_name(GOODBYE_ERROR), "error"), "the reasons' names: closed, lost-connection, error");
    char outdir[600], receipt[700];
    snprintf(outdir, sizeof outdir, "%s/out", root);
    t_mkdirs(outdir);
    snprintf(receipt, sizeof receipt, "%s/receipt.lua", outdir);
    const char *map = "/maps/the \"best\" map\\v2\nsecond line\001\tend 9 \xc3\xa9";
    Goodbye *g = NULL;
    e = goodbye_begin(receipt, map, &g, &why);
    check(!e && g, e ? why.text : "a run with a receipt begins");
    for (int i = 0; i < 3; ++i) goodbye_frame(g);
    e = goodbye_end(g, GOODBYE_CLOSED, NULL, &why);
    check(!e, e ? why.text : "it ends, closed, and the receipt is written");
    check(t_count_starting(outdir, ".receipt.lua.") == 0, "no temporary file is left beside it");
    t_luajit_read(receipt, got, sizeof got);
    t_hex(map, hex, sizeof hex);
    char started[24] = "", ended[24] = "", seconds[24] = "";
    char *f[8] = { 0 };
    int nf = 0;
    for (char *p = got, *s = got; nf < 8; ++p) if (*p == '|' || !*p) { int end = !*p; *p = '\0'; f[nf++] = s; s = p + 1; if (end) break; }
    if (nf == 8) { snprintf(started, sizeof started, "%s", f[2]); snprintf(ended, sizeof ended, "%s", f[3]); snprintf(seconds, sizeof seconds, "%s", f[4]); }
    snprintf(line, sizeof line, "LuaJIT reads it with loadfile in an empty environment: format 1, the map as given byte for byte (quotes, a backslash, a new line, a control byte, a tab, a digit after it, UTF-8), 3 frames, closed, no why (%.300s)",
             nf == 8 ? "read" : got);
    check(nf == 8 && !strcmp(f[0], "1") && !strcmp(f[1], hex) && !strcmp(f[5], "3") && !strcmp(f[6], "closed") && !strcmp(f[7], "nil"), line);
    snprintf(line, sizeof line, "started %s and ended %s are UTC times, in order; %s seconds between", started, ended, seconds);
    check(t_is_utc(started) && t_is_utc(ended) && strcmp(started, ended) <= 0 && atof(seconds) >= 0 && atof(seconds) < 60, line);
    e = goodbye_end(g, GOODBYE_CLOSED, NULL, &why);
    check(e == W_ERR_REFUSED && strstr(why.text, "written already"), "a second goodbye is refused");
    goodbye_free(g);
    char *text = t_read_text(receipt);
    int calls = 0;                                        /* a bracket outside the comment line would be a call */
    for (const char *l = text; l && *l; ) {
        const char *nl = strchr(l, '\n');
        size_t n = nl ? (size_t)(nl - l) : strlen(l);
        if (strncmp(l, "--", 2) && memchr(l, '(', n)) ++calls;
        l += n + (nl ? 1 : 0);
    }
    check(text && !calls && !strstr(text, "password") && !strncmp(text, "-- ", 3) && strstr(text, "\nreturn {\n    format  = 1,\n"),
          "the receipt holds plain data only (a comment, then return and one table: no names, no calls) and nothing of the password");

    e = goodbye_begin(receipt, "m", &g, &why);
    e = e ? e : goodbye_end(g, GOODBYE_ERROR, "a --test-map run reads the account's password from W_CLIENT_PASSWORD, and it is not set", &why);
    goodbye_free(g);
    t_luajit_read(receipt, got, sizeof got);
    check(!e && strstr(got, "|0|error|a --test-map run reads the account's password from W_CLIENT_PASSWORD, and it is not set"),
          e ? why.text : "an error ends it with its sentence as why, and replaces the earlier receipt whole");
    e = goodbye_begin(receipt, "m", &g, &why);
    e = e ? e : goodbye_end(g, GOODBYE_ERROR, NULL, &why);
    goodbye_free(g);
    t_luajit_read(receipt, got, sizeof got);
    check(!e && strstr(got, "|error|the client stopped on an error, with no sentence"), "an error with no sentence says that it has none");
    e = goodbye_begin(receipt, "m", &g, &why);
    e = e ? e : goodbye_end(g, GOODBYE_LOST_CONNECTION, "not written", &why);
    goodbye_free(g);
    t_luajit_read(receipt, got, sizeof got);
    check(!e && strstr(got, "|lost-connection|nil"), "lost-connection, with no why (a sentence is only kept with error)");
    /* }}} */

    /* {{{ a receipt that can't be written */
    snprintf(path, sizeof path, "%s/no-such-folder/receipt.lua", root);
    e = goodbye_begin(path, "m", &g, &why);
    check(e == W_ERR_NOT_FOUND && !g && strstr(why.text, "isn't there"), "a receipt in a folder that isn't there: refused at the start, before any window");
    check(goodbye_begin("", "m", &g, &why) == W_ERR_REFUSED && !g, "an empty receipt path is refused");
    snprintf(path, sizeof path, "%s/gone", root);
    t_mkdirs(path);
    snprintf(line, sizeof line, "%s/receipt.lua", path);
    e = goodbye_begin(line, "m", &g, &why);
    rmdir(path);
    e = e ? e : goodbye_end(g, GOODBYE_CLOSED, NULL, &why);
    check(e == W_ERR_IO && strstr(why.text, "temporary file") && strstr(why.text, "wasn't written"), "its folder taken away during the run: W_ERR_IO at the end, said so");
    goodbye_free(g);
    snprintf(path, sizeof path, "%s/a-folder.lua", outdir);
    t_mkdirs(path);
    e = goodbye_begin(path, "m", &g, &why);
    e = e ? e : goodbye_end(g, GOODBYE_CLOSED, NULL, &why);
    check(e == W_ERR_IO && strstr(why.text, "renamed") && t_count_starting(outdir, ".a-folder.lua.") == 0,
          "a receipt's name taken by a folder: the rename fails, W_ERR_IO, and the temporary file is removed");
    goodbye_free(g);
    check(goodbye_end(NULL, GOODBYE_CLOSED, NULL, &why) == W_OK, "a run with no receipt: goodbye_frame and goodbye_end do nothing");
    goodbye_frame(NULL);
    /* }}} */

    t_remove_tree(root);
}
/* }}} */

/* {{{ 1109: a converted map's folder (073, 074, 075): map.lua, its tables
 * per row ahead of the archives', heights from its own ground files, on
 * folders and an archive made here; then, when a converted build folder
 * is given (the third argument, else DIR/assets/test-map-build), the
 * real converter's output read the way a test-map run reads it. */

/* {{{ static the fixtures' writers */
/* a table (016-dbc.h): `rows` rows of `fields` cells, then its strings */
static size_t t_dbc(unsigned char *out, uint32_t rows, uint32_t fields, const uint32_t *cells, const char *text, size_t text_len)
{
    memcpy(out, "WDBC", 4);
    t_le32(out + 4, rows); t_le32(out + 8, fields); t_le32(out + 12, fields * 4); t_le32(out + 16, (uint32_t)text_len);
    for (uint32_t i = 0; i < rows * fields; ++i) t_le32(out + 20 + i * 4, cells[i]);
    memcpy(out + 20 + rows * fields * 4, text, text_len);
    return 20 + rows * fields * 4 + text_len;
}
/* map.lua of five parts (each a line of fields, "" to leave it out), read back */
static WErr t_conv(const char *folder, const char *format, const char *map, const char *start, const char *rule, const char *more, ConvertedMap *m, WWhy *why)
{
    char text[2000];
    snprintf(text, sizeof text, "-- made by the tests\nreturn {\n    %s\n    %s\n    %s\n    %s\n    %s\n}\n", format, map, start, rule, more);
    t_write_text(folder, "map.lua", text);
    return converted_map_read(folder, m, why);
}
/* the ground at (x, y) of the made tile below: a tilted plane */
static double t_plane(double x, double y) { return 3 + 0.01 * x - 0.02 * y; }
/* }}} */

static void converted_tests(const char *dir, const char *build)
{
    char root[400], path[900], line[1000];
    WWhy why;
    WErr e;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-converted", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the converted map's fixtures"); return; }
    t_remove_tree(root);

    /* {{{ Lua data, any file of it (059's reader, public) */
    {
        static const char GOOD[] = "-- a note\nreturn { n = -25, t = [[a\\b]], yes = true, no = false,\n  list = { 3, \"four\", { x = 1 } },\n  [\"key with space\"] = 7 }\n";
        LuaData *d = NULL;
        e = lua_data_parse(GOOD, strlen(GOOD), "made.lua", &d, &why);
        check(!e, e ? why.text : "plain Lua data reads (059's manifest reader, any file of it)");
        if (!e) {
            const LuaValue *top = lua_data_top(d), *list = lua_value_field(top, "list");
            check(lua_value_type(top) == LUA_DATA_TABLE && lua_value_number(lua_value_field(top, "n")) == -25
                  && !strcmp(lua_value_text(lua_value_field(top, "t")), "a\\b") && lua_value_true(lua_value_field(top, "yes"))
                  && lua_value_type(lua_value_field(top, "no")) == LUA_DATA_BOOL && !lua_value_true(lua_value_field(top, "no"))
                  && lua_value_number(lua_value_field(top, "key with space")) == 7,
                  "numbers, long-bracket text kept as written, true and false, a bracketed key");
            check(lua_value_listed(list) == 3 && lua_value_number(lua_value_item(list, 1)) == 3 && !strcmp(lua_value_text(lua_value_item(list, 2)), "four")
                  && lua_value_number(lua_value_field(lua_value_item(list, 3), "x")) == 1 && lua_value_item(list, 4) == NULL,
                  "a list's values in order from 1, a table inside it, nothing past its end");
            check(lua_value_line(list) == 3 && lua_value_field(top, "missing") == NULL && lua_value_type(NULL) == LUA_DATA_NIL && lua_value_text(lua_value_field(top, "n")) == NULL,
                  "each value's line; a missing field is nil; a number has no text");
        }
        lua_data_free(d);
        static const char CODE[] = "return {\n  a = 1,\n  b = os.exit(),\n}\n", CUT[] = "return { a = 1,\n";
        d = NULL;
        e = lua_data_parse(CODE, strlen(CODE), "code.lua", &d, &why);
        check(e == W_ERR_CORRUPT && strstr(why.text, "code.lua") && strstr(why.text, "3"), "a call is code, not data: refused, naming the file and line 3");
        lua_data_free(d);
        d = NULL;
        e = lua_data_parse(CUT, strlen(CUT), "cut.lua", &d, &why);
        check(e == W_ERR_CORRUPT, "a table never closed: corrupt");
        lua_data_free(d);
    }
    /* }}} */

    /* {{{ map.lua (073) */
    {
        char folder[600];
        snprintf(folder, sizeof folder, "%s/build", root);
        static const char FORMAT[] = "format = 1,";
        static const char MAP[] = "map = { id = 900, directory = \"WC3_Test\", name = \"A Test\", area = 9000, slot = 0, later = { 1, 2 } },";
        static const char START[] = "start = { x = -495.833, y = -441.667, z = 4.16667, player = 0, wc3 = { 13568, -15232 } },";
        static const char RULE[] = "wc3_to_wow = { scale = 0.0325521, x0 = 0, y0 = 0, rule = \"wow_x = x0 + scale * wc3_y\" },";
        static const char MORE[] = "creatures = \"world/creatures-900.tsv\", tiles = { first = { 31, 31 }, across = 2, down = 2 }, maps = { {} },";
        ConvertedMap m;
        t_mkdirs(folder);
        e = converted_map_read(folder, &m, &why);
        check(e == W_ERR_NOT_FOUND && strstr(why.text, "map.lua") && strstr(why.text, "W02c"), "a folder with no map.lua: not found, saying which converter writes it");
        e = t_conv(folder, FORMAT, MAP, START, RULE, MORE, &m, &why);
        check(!e, e ? why.text : "a map.lua as world-edit-to-execute's W02c writes it reads");
        check(m.id == 900 && !strcmp(m.directory, "WC3_Test") && !strcmp(m.name, "A Test") && m.area == 9000
              && near(m.start_x, -495.833, 1e-9) && near(m.start_y, -441.667, 1e-9) && m.has_z && near(m.start_z, 4.16667, 1e-9)
              && near(m.wc3_scale, 0.0325521, 1e-12) && !strcmp(m.creatures, "world/creatures-900.tsv"),
              "its map, folder, name, area, start (with its height), rule and creature list; fields it doesn't read passed over");
        double x, y;
        converted_map_from_wc3(&m, 13568, -15232, &x, &y);
        snprintf(line, sizeof line, "WC3's start (13568, -15232) by the map's rule is the start it gives (%.3f, %.3f)", x, y);
        check(near(x, m.start_x, 0.01) && near(y, m.start_y, 0.01), line);
        e = t_conv(folder, FORMAT, "map = { id = 1, directory = \"x\" },", "start = { x = 0, y = 0 },", RULE, "", &m, &why);
        check(!e && m.name[0] == 0 && m.area == 0 && !m.has_z && m.creatures[0] == 0, e ? why.text : "name, area, height and creatures may be left out");
        static const struct { const char *format, *map, *start, *rule, *more; WErr code; const char *says; } BAD[] = {
            { "format = 2,", MAP, START, RULE, MORE, W_ERR_UNSUPPORTED, "format 2" },
            { "", MAP, START, RULE, MORE, W_ERR_REFUSED, "has no format" },
            { FORMAT, "map = { id = 70000, directory = \"WC3_Test\" },", START, RULE, MORE, W_ERR_REFUSED, "map.id is 70000" },
            { FORMAT, "map = { id = 900.5, directory = \"WC3_Test\" },", START, RULE, MORE, W_ERR_REFUSED, "map.id is 900.5" },
            { FORMAT, "map = { id = \"900\", directory = \"WC3_Test\" },", START, RULE, MORE, W_ERR_REFUSED, "map.id is a string" },
            { FORMAT, "map = { id = 900, directory = \"../Kalimdor\" },", START, RULE, MORE, W_ERR_REFUSED, "map.directory \"../Kalimdor\"" },
            { FORMAT, "map = { id = 900, directory = [[World\\Maps\\X]] },", START, RULE, MORE, W_ERR_REFUSED, "never a path" },
            { FORMAT, "map = { id = 900, directory = \"\" },", START, RULE, MORE, W_ERR_REFUSED, "map.directory" },
            { FORMAT, "map = { id = 900, directory = \"A123456789012345678901234567890123456789012345678\" },", START, RULE, MORE, W_ERR_REFUSED, "at most 48" },
            { FORMAT, MAP, "", RULE, MORE, W_ERR_REFUSED, "has no start" },
            { FORMAT, MAP, "start = { x = 1 },", RULE, MORE, W_ERR_REFUSED, "start has no y" },
            { FORMAT, MAP, "start = { x = 20000, y = 0 },", RULE, MORE, W_ERR_REFUSED, "off the world" },
            { FORMAT, MAP, START, "wc3_to_wow = { scale = 0, x0 = 0, y0 = 0 },", MORE, W_ERR_REFUSED, "wc3_to_wow.scale is 0" },
            { FORMAT, MAP, START, "", MORE, W_ERR_REFUSED, "has no wc3_to_wow" },
            { FORMAT, MAP, START, RULE, "creatures = \"../../server/world.sql\",", W_ERR_REFUSED, "isn't a path inside the build folder" },
            { FORMAT, MAP, START, RULE, "creatures = \"/etc/passwd\",", W_ERR_REFUSED, "isn't a path inside" },
            { FORMAT, MAP, START, RULE, "creatures = [[C:\\x.tsv]],", W_ERR_REFUSED, "isn't a path inside" },
            { FORMAT, MAP, START, RULE, "creatures = 5,", W_ERR_REFUSED, "creatures is a number" },
        };
        int worked = 0, n = (int)(sizeof BAD / sizeof BAD[0]);
        for (int i = 0; i < n; ++i) {
            e = t_conv(folder, BAD[i].format, BAD[i].map, BAD[i].start, BAD[i].rule, BAD[i].more, &m, &why);
            if (e == BAD[i].code && strstr(why.text, BAD[i].says) && strstr(why.text, "map.lua line") && m.id == 0) ++worked;
            else printf("  (map.lua refusal %d: code %d, \"%s\"; wanted %d, \"%s\")\n", i + 1, e, e ? why.text : "", BAD[i].code, BAD[i].says);
        }
        snprintf(line, sizeof line, "%d of %d broken map.lua files refused with the field and its line, nothing kept", worked, n);
        check(worked == n, line);
        t_write_text(folder, "map.lua", "return 900\n");
        e = converted_map_read(folder, &m, &why);
        check(e == W_ERR_REFUSED && strstr(why.text, "returns a number"), "a map.lua returning a number, not a table: refused");
        t_write_text(folder, "map.lua", "return { format = 1, map = { id = 900, directory = 'x' } \n");
        e = converted_map_read(folder, &m, &why);
        check(e == W_ERR_CORRUPT, "a map.lua that isn't Lua data: corrupt");
    }
    /* }}} */

    /* {{{ the tables, per row, through the resolver (074; issue 106) */
    {
        char data[520], overlay[520];
        snprintf(data, sizeof data, "%s/Data", root);
        snprintf(overlay, sizeof overlay, "%s/build", root);
        t_mkdirs(data);
        /* the archives' Map: Azeroth 0, Kalimdor 1, Outland 530; their AreaTable 3 fields wide */
        static unsigned char amap[200], aarea[100], omap[200], oarea[100];
        static const char ATEXT[] = "\0Azeroth\0Kalimdor\0Expansion01";            /* 1, 9, 18 */
        const uint32_t acells[] = { 0, 1, 0,   1, 9, 1,   530, 18, 2 };
        size_t amap_n = t_dbc(amap, 3, 3, acells, ATEXT, sizeof ATEXT);
        const uint32_t aarea_cells[] = { 14, 1, 0 };
        size_t aarea_n = t_dbc(aarea, 1, 3, aarea_cells, "", 1);
        snprintf(path, sizeof path, "%s/patch.MPQ", data);
        const char *const names[] = { "DBFilesClient\\Map.dbc", "DBFilesClient\\AreaTable.dbc" };
        const char *const bytes[] = { (const char *)amap, (const char *)aarea };
        const size_t lens[] = { amap_n, aarea_n };
        t_write_mpq_n(path, names, bytes, lens, 2);
        /* the map's: its own map 900, and a row 1 of its own, which wins; an AreaTable 2 fields wide */
        static const char OTEXT[] = "\0WC3_Test\0Override";                       /* 1, 10 */
        const uint32_t ocells[] = { 900, 1, 7,   1, 10, 8 };
        size_t omap_n = t_dbc(omap, 2, 3, ocells, OTEXT, sizeof OTEXT);
        const uint32_t oarea_cells[] = { 9000, 0 };
        size_t oarea_n = t_dbc(oarea, 1, 2, oarea_cells, "", 1);
        t_write(overlay, "DBFILESCLIENT/map.DBC", omap, omap_n);
        t_write(overlay, "DBFilesClient/AreaTable.dbc", oarea, oarea_n);

        MpqChain chain;
        e = mpq_chain_open(&chain, data, "enUS", &why);
        check(!e, e ? why.text : "a made-up archive of two tables opens");
        if (e) { t_remove_tree(root); return; }
        ResolverPlace place = { .noverlays = 1, .chain = &chain, .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", overlay);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        e = resolver_open(&place, &r, &why);
        check(!e, e ? why.text : "the resolver opens the map's folder as an overlay over the archive");
        if (!e) {
            AssetAnswer all[4];
            int n = 0;
            e = asset_resolve_all(r, "DBFilesClient\\Map.dbc", ASSET_TABLE, all, 4, &n, &why);
            check(!e && n == 2 && all[0].source == ASSET_FROM_OVERLAY && all[0].len == omap_n && all[1].source == ASSET_FROM_BLIZZARD && all[1].len == amap_n,
                  e ? why.text : "every place's copy of a table, the map's first, then the archives'");
            for (int i = 0; i < n; ++i) asset_answer_free(&all[i]);
            e = asset_resolve_all(r, "DBFilesClient\\Map.dbc", ASSET_TABLE, all, 1, &n, &why);
            check(e == W_ERR_FULL, "more copies than room for: full, not some of them");
            AssetAnswer a;
            AssetCounts before, after;
            asset_counts(r, &before);
            e = asset_resolve(r, "DBFilesClient\\Nowhere.dbc", ASSET_TABLE, &a, &why);
            WErr e2 = asset_read(r, NULL, "World\\Maps\\X\\X.wdt", ASSET_GROUND, &a, &why);
            asset_counts(r, &after);
            fflush(place.log);
            check(e == W_ERR_NOT_FOUND && e2 == W_ERR_NOT_FOUND && after.placeholder_answers == before.placeholder_answers && (!log || !strstr(log, "stands in")),
                  "a table or a ground file nowhere: not found, quietly; nothing stands in (a placeholder table or ground would be wrong)");
            e = asset_read(r, NULL, "World\\Maps\\X\\X.wdt", ASSET_TEXTURE, &a, &why);
            check(e == W_ERR_NOT_FOUND, "asset_read never gives a placeholder, of any kind");
            e = asset_read(NULL, &chain, "DBFilesClient\\Map.dbc", ASSET_TABLE, &a, &why);
            check(!e && a.source == ASSET_FROM_BLIZZARD && a.len == amap_n, e ? why.text : "asset_read with no resolver: the archives alone");
            asset_answer_free(&a);

            Tables *ts = tables_new(r, &chain);
            TableRow row;
            e = tables_row(ts, "Map", 900, &row, &why);
            check(!e && row.source == ASSET_FROM_OVERLAY && !strcmp(row.from, overlay) && !strcmp(dbc_text(row.dbc, row.row, 2), "WC3_Test") && dbc_u32(row.dbc, row.row, 3) == 7,
                  e ? why.text : "the map's own row (900) from its folder");
            e = tables_row(ts, "Map", 1, &row, &why);
            check(!e && row.source == ASSET_FROM_OVERLAY && !strcmp(dbc_text(row.dbc, row.row, 2), "Override"), "a row both have: the map's wins (106: the pack's rows first)");
            e = tables_row(ts, "Map", 530, &row, &why);
            check(!e && row.source == ASSET_FROM_BLIZZARD && !strcmp(dbc_text(row.dbc, row.row, 2), "Expansion01") && row.row == 2,
                  "a row only the archives have: still theirs, per row");
            e = tables_row(ts, "Map", 0, &row, &why);
            check(!e && row.source == ASSET_FROM_BLIZZARD && row.row == 0, "row 0 too");
            e = tables_row(ts, "Map", 77, &row, &why);
            check(e == W_ERR_NOT_FOUND && strstr(why.text, "no row 77 in Map") && strstr(why.text, "2 copies: 4 rows"), e ? why.text : "a row no copy has: not found");
            uint32_t rows[TABLES_COPIES_MAX];
            AssetSource places[TABLES_COPIES_MAX];
            check(tables_copies(ts, "Map", rows, places) == 2 && rows[0] == 2 && rows[1] == 3 && places[0] == ASSET_FROM_OVERLAY && places[1] == ASSET_FROM_BLIZZARD,
                  "two copies of Map, the map's 2 rows first, the archives' 3");
            e = tables_row(ts, "AreaTable", 9000, &row, &why);
            check(e == W_ERR_REFUSED && strstr(why.text, "2 fields") && strstr(why.text, "3") && strstr(why.text, "another client build"),
                  e ? why.text : "copies of different widths: refused, naming both (106's acceptance)");
            WWhy again;
            check(tables_row(ts, "AreaTable", 14, &row, &again) == W_ERR_REFUSED && !strcmp(again.text, why.text) && tables_copies(ts, "AreaTable", NULL, NULL) == -1,
                  "and refused the same way each time it is asked");
            e = tables_row(ts, "Faction", 1, &row, &why);
            check(e == W_ERR_NOT_FOUND && strstr(why.text, "Faction.dbc is in none"), e ? why.text : "a table no place has: not found, named");
            e = tables_row(ts, "", 1, &row, &why);
            check(e == W_ERR_REFUSED, "an empty table name: refused");
            char *rep = NULL;
            size_t rep_len = 0;
            FILE *to = open_memstream(&rep, &rep_len);
            tables_report(ts, to);
            fclose(to);
            check(rep && strstr(rep, "Map") && strstr(rep, "overlay") && strstr(rep, "blizzard") && strstr(rep, "not read"), "the report: each table's copies and places, and the one not read");
            free(rep);
            tables_free(ts);

            ts = tables_new(NULL, &chain);
            e = tables_row(ts, "Map", 1, &row, &why);
            check(!e && row.source == ASSET_FROM_BLIZZARD && !strcmp(dbc_text(row.dbc, row.row, 2), "Kalimdor") && tables_copies(ts, "Map", NULL, NULL) == 1,
                  e ? why.text : "with no resolver (every run but a test map's): the archives' rows, one copy");
            e = tables_row(ts, "Map", 900, &row, &why);
            check(e == W_ERR_NOT_FOUND, "and the map's rows aren't there");
            tables_free(ts);
            resolver_free(r);
        }
        fclose(place.log);
        free(log);
        mpq_chain_close(&chain);
    }
    /* }}} */

    /* {{{ heights from a map's own ground files (075) */
    {
        AdtTile *adt = calloc(1, sizeof *adt);
        const int tx = 31, ty = 32;
        for (int k = 0; k < 256; ++k) {
            AdtChunk *c = &adt->chunks[k];
            int cy = k / 16, cx = k % 16;
            c->ix = cx; c->iy = cy;
            c->z = 2;                                     /* the base, under every height */
            float x, y;
            for (int rr = 0; rr <= 8; ++rr)
                for (int cc = 0; cc <= 8; ++cc) {
                    maps_corner_position(tx, ty, (float)(cy * 8 + rr), (float)(cx * 8 + cc), &x, &y);
                    c->heights[adt_corner(rr, cc)] = (float)(t_plane(x, y) - 2);
                }
            for (int rr = 0; rr < 8; ++rr)
                for (int cc = 0; cc < 8; ++cc) {
                    maps_corner_position(tx, ty, cy * 8 + rr + 0.5f, cx * 8 + cc + 0.5f, &x, &y);
                    c->heights[adt_centre(rr, cc)] = (float)(t_plane(x, y) - 2);
                }
        }
        adt->chunks[2 * 16 + 3].holes = 0xFFFF;           /* chunk row 2, column 3: all hole */
        MapTile *t = NULL;
        e = adt_heights_from_tile(adt, tx, ty, &t, &why);
        check(!e && t && t->tx == tx && t->ty == ty && !t->is_flat && t->has_holes, e ? why.text : "a ground file's tile as the server's map file would hold it");
        if (t) {
            float worst = 0, gx, gy;
            int tried = 0;
            for (int i = 0; i < 400; ++i) {
                float x, y;
                maps_corner_position(tx, ty, 0.37f + (float)i * 0.317f, 127.6f - (float)i * 0.311f, &x, &y);
                int ix = (int)(0.37f + (float)i * 0.317f), iy = (int)(127.6f - (float)i * 0.311f);
                if (maps_is_hole(t, ix, iy)) continue;
                float z = maps_height_in_tile(t, x, y, &gx, &gy);
                if (fabsf(z - (float)t_plane(x, y)) > worst) worst = fabsf(z - (float)t_plane(x, y));
                ++tried;
            }
            snprintf(line, sizeof line, "the ground is the ground file's: a tilted plane comes back as itself at %d places (worst %.5f yards)", tried, worst);
            check(tried > 350 && worst < 0.01f, line);
            float x, y;
            maps_corner_position(tx, ty, 2 * 8 + 3.5f, 3 * 8 + 4.5f, &x, &y);
            check(maps_is_hole(t, 2 * 8 + 3, 3 * 8 + 4) && maps_height_in_tile(t, x, y, &gx, &gy) == MAPS_INVALID_HEIGHT && !maps_is_hole(t, 2 * 8 + 3, 2 * 8 + 4),
                  "a chunk's holes are the tile's holes there, and only there");
            maps_corner_position(tx, ty, 8, 8, &x, &y);
            check(t->v9[8 * 129 + 8] == (float)t_plane(x, y), "a corner chunks share: the same height from either");
            maps_free_tile(t);
        }
        adt->chunks[17].ix = 5;
        e = adt_heights_from_tile(adt, tx, ty, &t, &why);
        check(e == W_ERR_CORRUPT && !t && strstr(why.text, "chunk 17"), "a chunk out of its place: corrupt, named");
        free(adt);
    }
    /* }}} */

    t_remove_tree(root);

    /* {{{ the converter's own output, when there is some */
    char conv_file[700];
    snprintf(conv_file, sizeof conv_file, "%s/map.lua", build);
    struct stat sb;
    if (stat(conv_file, &sb)) {
        printf("  (no converted build folder at %s: its checks are skipped; make one with world-edit-to-execute's wow-convert.lua)\n", build);
        return;
    }
    printf("  a converted build folder: %s\n", build);
    ConvertedMap m;
    e = converted_map_read(build, &m, &why);
    check(!e, e ? why.text : "its map.lua reads");
    if (e) return;
    ResolverPlace place = { .noverlays = 1, .distinctness = 0.5 };
    snprintf(place.overlays[0], sizeof place.overlays[0], "%s", build);
    char *conv_log = NULL;
    size_t conv_log_len = 0;
    place.log = open_memstream(&conv_log, &conv_log_len);   /* the placeholders' warnings (thousands of lines for a big map): counted below */
    Resolver *r = NULL;
    e = resolver_open(&place, &r, &why);
    check(!e, e ? why.text : "the resolver opens the build folder as the test map's overlay (no archives)");
    if (e) { fclose(place.log); free(conv_log); return; }
    Tables *ts = tables_new(r, NULL);
    TableRow row;
    e = tables_row(ts, "Map", (uint32_t)m.id, &row, &why);
    snprintf(line, sizeof line, "its Map row %d is in its folder's Map.dbc and names its folder %s", m.id, m.directory);
    check(!e && row.source == ASSET_FROM_OVERLAY && !strcasecmp(dbc_text(row.dbc, row.row, 2), m.directory), e ? why.text : line);
    static const struct { const char *name; uint32_t fields; } WIDTH[] = {
        { "Map", 66 }, { "AreaTable", 36 }, { "CreatureDisplayInfo", 16 }, { "CreatureModelData", 28 }, { "Faction", 57 }, { "FactionTemplate", 14 }, { "LoadingScreens", 4 },
    };
    int wide = 0, nwidth = (int)(sizeof WIDTH / sizeof WIDTH[0]);
    for (int i = 0; i < nwidth; ++i) {
        AssetAnswer a;
        Dbc d;
        snprintf(path, sizeof path, "DBFilesClient\\%s.dbc", WIDTH[i].name);
        if (asset_read(r, NULL, path, ASSET_TABLE, &a, &why)) { printf("  (%s: %s)\n", WIDTH[i].name, why.text); continue; }
        if (!dbc_open(&d, a.data, a.len, &why) && d.fields == WIDTH[i].fields) ++wide;
        else printf("  (%s: %u fields, 3.3.5a's has %u)\n", WIDTH[i].name, d.fields, WIDTH[i].fields);
        asset_answer_free(&a);
    }
    snprintf(line, sizeof line, "%d of its %d tables are as wide as 3.3.5a's own, so its rows sit beside the archives' (074 refuses another width)", wide, nwidth);
    check(wide == nwidth, line);

    LuaData *d = NULL;
    e = lua_data_read(conv_file, CONVERTED_MAP_MOST, &d, &why);
    const LuaValue *wc3 = e ? NULL : lua_value_field(lua_value_field(lua_data_top(d), "start"), "wc3");
    if (lua_value_listed(wc3) >= 2) {
        double x, y;
        converted_map_from_wc3(&m, lua_value_number(lua_value_item(wc3, 1)), lua_value_number(lua_value_item(wc3, 2)), &x, &y);
        snprintf(line, sizeof line, "the start (%.3f, %.3f) is WC3's start by the map's own rule (%.3f, %.3f)", m.start_x, m.start_y, x, y);
        check(near(x, m.start_x, 0.01) && near(y, m.start_y, 0.01), line);
    }
    lua_data_free(d);

    AdtHeights *h = NULL;
    e = adt_heights_open(r, NULL, m.directory, &h, &why);
    check(!e, e ? why.text : "its WDT is in its folder");
    if (!e) {
        MapWorld *w = malloc(sizeof *w);
        maps_world_init_source(w, m.id, adt_heights_tile, h, "the map's own ground files");
        float gx, gy;
        float z = maps_world_height(w, (float)m.start_x, (float)m.start_y, &gx, &gy);
        snprintf(line, sizeof line, "the ground at the start, from its own ground files: %.3f yards (map.lua's start.z %.3f)", z, m.has_z ? m.start_z : 0.0);
        check(z > -10000 && (!m.has_z || near(z, m.start_z, 0.5)), line);
        SpawnList sl = { 0 };
        snprintf(path, sizeof path, "%s/%s", build, m.creatures[0] ? m.creatures : "world/creatures.tsv");
        e = creatures_read(path, &sl, &why);
        int on = 0;
        float off = 0;
        for (int i = 0; !e && i < sl.n; ++i) {
            float g = maps_world_height(w, (float)sl.spawns[i].x, (float)sl.spawns[i].y, &gx, &gy);
            if (g < -10000) continue;
            ++on;
            if (fabsf(g - (float)sl.spawns[i].z) > off) off = fabsf(g - (float)sl.spawns[i].z);
        }
        snprintf(line, sizeof line, "its %d creatures (%s) read, %d of them on its ground, the farthest %.2f yards off it", sl.n, m.creatures, on, off);
        if (!e && sl.n == 0) printf("  (its creature list %s is empty: a map with no units; their checks are skipped)\n", m.creatures);
        else check(!e && sl.n > 0 && on == sl.n && off < 2, e ? why.text : line);
        creatures_free(&sl);
        snprintf(line, sizeof line, "%d of its ground tiles made from its ADTs", adt_heights_made(h));
        check(adt_heights_made(h) >= 1, line);
        /* its ground's pictures through the resolver (1109 step 1c): the
         * converter names the game's own ground pictures, which a build
         * folder doesn't hold, so with no archives each is the checker */
        int ga = -1, gb = -1;
        for (int a1 = 0; a1 < 64 && ga < 0; ++a1)
            for (int b1 = 0; b1 < 64 && ga < 0; ++b1) {
                AssetAnswer a;
                snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", m.directory, m.directory, a1, b1);
                if (!asset_read(r, NULL, path, ASSET_GROUND, &a, &why)) { ga = a1; gb = b1; asset_answer_free(&a); }
            }
        GroundPlace gp;
        GroundPrep *gpr = NULL;
        e = ground_place_init(&gp, NULL, r, m.directory, w, 0, 0, &why);
        if (!e) e = ground_prep_tile(&gp, gb, ga, NULL, NULL, &gpr, &why);       /* its file is <dir>_<a>_<b>: the tile (b, a) (033) */
        int need = 0, checkers = 0;
        for (int k = 0; !e && k < gpr->adt->ntextures; ++k) {
            if (!gpr->needed[k]) continue;
            ++need;
            if (gpr->pixels[k].rgba && gpr->pixels[k].width == 8 && gpr->pixels[k].rgba[0] == 255 && gpr->pixels[k].rgba[1] == 0) ++checkers;
        }
        snprintf(line, sizeof line, "its ground tile %d_%d prepared with no archives: the %d ground pictures it names each stand in as the checker (%d)", ga, gb, need, checkers);
        check(!e && need > 0 && checkers == need, e ? why.text : line);
        if (gpr) ground_prep_free(gpr, 0);
        maps_world_free(w);
        free(w);
        adt_heights_free(h);
    }

    /* its own WC3 model: the creature looks that name an .mdx it brought */
    AssetAnswer info;
    int named = 0, read = 0, kept = 0, looks = 0;
    char first[300] = "";
    if (!asset_read(r, NULL, "DBFilesClient\\CreatureDisplayInfo.dbc", ASSET_TABLE, &info, &why)) {
        Dbc di;
        if (!dbc_open(&di, info.data, info.len, &why))
            for (uint32_t i = 0; i < di.rows; ++i, ++looks) {
                uint32_t model = dbc_u32(&di, i, 2);
                if (tables_row(ts, "CreatureModelData", model, &row, &why)) continue;
                const char *file = dbc_text(row.dbc, row.row, 3);
                AssetAnswer a;
                if (asset_read(r, NULL, file, ASSET_MODEL, &a, &why)) continue;       /* a game model's .mdx name: the archives' .m2 (041) */
                ++named;
                if (!first[0]) snprintf(first, sizeof first, "%s", file);
                MdxModel x;
                if (a.len >= 4 && !memcmp(a.data, "MDLX", 4) && !mdx_read(a.data, a.len, &x, &why)) { ++read; if (x.model.nunits > 0) ++kept; mdx_free(&x); }
                asset_answer_free(&a);
            }
        asset_answer_free(&info);
    }
    snprintf(line, sizeof line, "%d creature looks name a WC3 model the map brought (first %s); %d read as MDX by 061, %d with something to draw", named, first[0] ? first : "none", read, kept);
    if (looks == 0) printf("  (its CreatureDisplayInfo has no rows: a map with no units; this check is skipped)\n");
    else check(named > 0 && read == named && kept == named, line);
    /* {{{ its doodads and buildings (1109 step 1c): every placement the
     * converter wrote, read back from its ADTs where placements.lua says
     * it stands, and every model placed found: its own WC3 models (and
     * their pictures) from its folder, the game's (no archives here) the
     * box standing in */
    {
        LuaData *pd = NULL;
        snprintf(path, sizeof path, "%s/placements.lua", build);
        e = lua_data_read(path, (size_t)1 << 28, &pd, &why);
        check(!e, e ? why.text : "its placements.lua reads with the data reader");
        const LuaValue *list = e ? NULL : lua_value_field(lua_data_top(pd), "doodads");
        int nlist = lua_value_listed(list), placed = 0, by_map = 0;
        double (*where)[3] = calloc((size_t)nlist + 1, sizeof *where);
        unsigned char *wanted = calloc((size_t)nlist + 1, 1);
        for (int i = 1; i <= nlist; ++i) {
            const LuaValue *dv = lua_value_item(list, i), *at = lua_value_field(dv, "world");
            int uid = (int)lua_value_number(lua_value_field(dv, "uid"));
            if (lua_value_listed(at) < 3 || uid < 1 || uid > nlist) continue;
            ++placed;
            const char *src = lua_value_text(lua_value_field(dv, "source"));
            if (src && !strcmp(src, "map")) ++by_map;
            wanted[uid] = 1;
            for (int k = 0; k < 3; ++k) where[uid][k] = lua_value_number(lua_value_item(at, k + 1));
        }
        AssetAnswer wdt;
        snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdt", m.directory, m.directory);
        int big = asset_read(r, NULL, path, ASSET_GROUND, &wdt, &why) ? -1 : adt_wdt_big_alpha(wdt.data, wdt.len, &why);
        if (big >= 0) asset_answer_free(&wdt);
        unsigned char *seen = calloc((size_t)nlist + 1, 1);
        int tiles = 0, found = 0, far_off = 0, unknown = 0, nbuild = 0;
        float worst = 0;
        char (*names)[260] = NULL;
        int nnames = 0, cap = 0;
        AdtTile *t = malloc(sizeof *t);
        for (int tx = 0; big >= 0 && tx < 64; ++tx)
            for (int ty = 0; ty < 64; ++ty) {
                AssetAnswer a;
                snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", m.directory, m.directory, tx, ty);
                if (asset_read(r, NULL, path, ASSET_GROUND, &a, &why)) continue;
                WErr bad = adt_read(a.data, a.len, big, t, &why);
                asset_answer_free(&a);
                if (bad) { check(0, why.text); continue; }
                ++tiles;
                nbuild += t->nplaces;
                for (int k = 0; k < t->ndoodads; ++k) {
                    const AdtPlace *p = &t->doodads[k];
                    if (p->uid < 1 || p->uid > (uint32_t)nlist || !wanted[p->uid]) { ++unknown; continue; }
                    if (seen[p->uid]) continue;            /* listed by two tiles: one doodad */
                    seen[p->uid] = 1;
                    ++found;
                    float mm[16];
                    adt_place_matrix(p, 0, 0, mm);
                    float off = fmaxf(fmaxf(fabsf(mm[12] - (float)where[p->uid][0]), fabsf(mm[13] - (float)where[p->uid][1])), fabsf(mm[14] - (float)where[p->uid][2]));
                    if (off > worst) worst = off;
                    if (off > 0.02f) ++far_off;
                    const char *name = t->models[p->name];
                    int known = 0;
                    for (int q = 0; q < nnames && !known; ++q) known = !strcasecmp(names[q], name);
                    if (known) continue;
                    if (nnames == cap) { cap = cap ? cap * 2 : 64; names = realloc(names, sizeof *names * (size_t)cap); }
                    snprintf(names[nnames++], sizeof names[0], "%s", name);
                }
                adt_free(t);
            }
        free(t);
        snprintf(line, sizeof line, "its %d ADT tiles place %d different doodads, the %d placements.lua says were placed (%d by the map's own models); %d unknown to it",
                 tiles, found, placed, by_map, unknown);
        check(tiles > 0 && found == placed && unknown == 0, line);
        snprintf(line, sizeof line, "each where placements.lua says (the farthest %.4f yards off; %d past 0.02)", worst, far_off);
        check(found > 0 && far_off == 0, line);
        if (nbuild) printf("  (%d building placements: no stock row is a building yet, W02b's decision 2)\n", nbuild);

        /* every model placed, through the resolver as the drawer reads it */
        int own = 0, own_pieces = 0, boxes = 0, failed = 0, refused = 0, pics = 0, pics_own = 0, pics_box = 0, pics_bad = 0, replaceable = 0, hidden = 0;
        float lo_h = 1e30f, hi_h = 0;
        char tall[260] = "";
        for (int q = 0; q < nnames; ++q) {
            PlacedModel pm;
            if (placed_model_read(r, NULL, names[q], m.wc3_scale, &pm, &why)) {
                /* a WC3 model of its own that 061 refuses (the drawer leaves it out, said) */
                AssetAnswer a;
                int wc3 = !asset_read(r, NULL, names[q], ASSET_MODEL, &a, &why) && a.len >= 4 && !memcmp(a.data, "MDLX", 4);
                if (wc3) asset_answer_free(&a);
                if (wc3) ++refused;
                printf("  (%s %s)\n", wc3 ? "its own WC3 model, refused by 061:" : "unreadable:", why.text);
                ++failed;
                continue;
            }
            const M2Model *d = placed_model_drawn(&pm);
            if (pm.source == ASSET_FROM_PLACEHOLDER) ++boxes;
            if (pm.wc3) {
                ++own;
                float zlo = 1e30f, zhi = -1e30f;
                for (uint32_t i = 0; i < d->nvertices; ++i) { if (d->vertices[i].z < zlo) zlo = d->vertices[i].z; if (d->vertices[i].z > zhi) zhi = d->vertices[i].z; }
                if (d->nvertices && zhi - zlo < lo_h) lo_h = zhi - zlo;
                if (d->nvertices && zhi - zlo > hi_h) { hi_h = zhi - zlo; snprintf(tall, sizeof tall, "%s", names[q]); }
                int drawn = 0, left = 0;
                for (uint32_t u = 0; u < d->nunits; ++u) {
                    const M2Unit *un = &d->units[u];
                    if (un->texture >= d->ntextures) { ++left; continue; }
                    if (un->alpha <= 0) { ++hidden; ++left; continue; }
                    if (d->textures[un->texture].kind != 0) { ++replaceable; ++left; continue; }
                    ++drawn;
                }
                if (drawn) ++own_pieces;
                else if (!d->nunits) {
                    int fx = 0;
                    for (uint32_t k = 0; k < pm.mdx.nnodes; ++k) fx += pm.mdx.nodes[k].kind == MDX_EMITTER || pm.mdx.nodes[k].kind == MDX_EMITTER2 || pm.mdx.nodes[k].kind == MDX_RIBBON;
                    printf("  (%s has no geosets to draw: %d particle emitters and ribbons alone, not drawn yet: 403)\n", names[q], fx);
                }
                else printf("  (%s: none of its %u pieces drawn at rest: team colour or hidden)\n", names[q], d->nunits);
                for (uint32_t k = 0; k < d->ntextures; ++k) {
                    if (d->textures[k].kind != 0 || !d->textures[k].file[0]) continue;
                    AssetAnswer a;
                    ++pics;
                    if (asset_get(r, NULL, d->textures[k].file, ASSET_TEXTURE, &a, &why)) { ++pics_bad; continue; }
                    if (a.source == ASSET_FROM_PLACEHOLDER) ++pics_box; else if (a.source == ASSET_FROM_OVERLAY) ++pics_own;
                    BlpImage img;
                    if (blp_decode(a.data, a.len, &img, &why)) { ++pics_bad; printf("  (%s: %s)\n", d->textures[k].file, why.text); }
                    else blp_free(&img);
                    asset_answer_free(&a);
                }
            }
            placed_model_free(&pm);
        }
        snprintf(line, sizeof line, "its %d different models placed: %d its own WC3 models read from its folder (%d with a piece to draw at rest), %d the game's, each the box here (no archives); %d of its own refused by 061, nothing else unreadable",
                 nnames, own, own_pieces, boxes, refused);
        check(nnames > 0 && failed == refused && own + boxes + refused == nnames && (own ? own_pieces > 0 : own_pieces == 0) && own_pieces * 10 >= own * 9 && (by_map == 0) == (own + refused == 0), line);
        if (own) {
            snprintf(line, sizeof line, "its own models stand %.2f to %.2f yards tall by its rule (%.5f yards a WC3 unit; the tallest %s)", lo_h, hi_h, m.wc3_scale, tall);
            check(lo_h >= 0 && hi_h > 0.5f && hi_h < 200, line);
            snprintf(line, sizeof line, "the %d pictures they name: %d from its folder, %d the checker (WC3's own, not in a build folder), all %s; %d pieces left out for team colour (404), %d hidden at rest",
                     pics, pics_own, pics_box, pics_bad ? "but some decoded" : "decoded", replaceable, hidden);
            check(pics_own > 0 && pics_bad == 0 && pics_own + pics_box == pics, line);
        }
        free(names); free(seen); free(where); free(wanted);
        lua_data_free(pd);
    }
    /* }}} */

    tables_free(ts);
    AssetCounts cc;
    asset_counts(r, &cc);
    printf("  (the placeholders stood in %u times, each said in the resolver's log; %u different models and %u pictures from its folder)\n",
           cc.placeholder_answers, cc.by[ASSET_MODEL][ASSET_FROM_OVERLAY], cc.by[ASSET_TEXTURE][ASSET_FROM_OVERLAY]);
    resolver_free(r);
    fclose(place.log);
    free(conv_log);
    /* }}} */
}
/* }}} */

/* {{{ placed models (076; issue 1109 step 1c): a doodad's and a building's
 * files through the resolver, a WC3 model the map brought, the box
 * standing in, ground pictures from the map's folder; folders and an
 * archive made here, no game data */
/* a building made here: a root naming one part and one material, its
 * picture Textures\Hut.blp; a part of one triangle (tags reversed, as the
 * files hold them: 021-wmo.h) */
static void t_made_wmo_root(Bytes *o)
{
    o->n = 0;
    size_t c = open_chunk(o, "REVM"); put32(o, 17); close_chunk(o, c);
    c = open_chunk(o, "DHOM");
    put32(o, 1); put32(o, 1);                             /* materials, groups */
    for (int i = 2; i < 16; ++i) put32(o, 0);
    close_chunk(o, c);
    c = open_chunk(o, "XTOM"); putname(o, "Textures\\Hut.blp", 20); close_chunk(o, c);
    c = open_chunk(o, "TMOM");
    put32(o, 0); put32(o, 0); put32(o, 0); put32(o, 0);   /* flags, shader, blend 0, picture at MOTX 0 */
    for (int i = 4; i < 16; ++i) put32(o, 0);
    close_chunk(o, c);
}
static void t_made_wmo_group(Bytes *o)
{
    o->n = 0;
    size_t g = open_chunk(o, "REVM"); put32(o, 17); close_chunk(o, g);   /* (the reader wants MOGP after a first block, as the files have) */
    g = open_chunk(o, "PGOM");
    for (int i = 0; i < 17; ++i) put32(o, 0);             /* the 68-byte header */
    size_t c = open_chunk(o, "YPOM"); for (int i = 0; i < 1; ++i) put16(o, 0x20); close_chunk(o, c);
    c = open_chunk(o, "IVOM"); put16(o, 0); put16(o, 1); put16(o, 2); close_chunk(o, c);
    c = open_chunk(o, "TVOM");
    static const float v[9] = { 0, 0, 0, 4, 0, 0, 0, 4, 3 };
    for (int i = 0; i < 9; ++i) putf(o, v[i]);
    close_chunk(o, c);
    c = open_chunk(o, "RNOM"); for (int i = 0; i < 3; ++i) { putf(o, 0); putf(o, 0); putf(o, 1); } close_chunk(o, c);
    c = open_chunk(o, "VTOM"); for (int i = 0; i < 6; ++i) putf(o, 0); close_chunk(o, c);
    c = open_chunk(o, "ABOM"); for (int i = 0; i < 3; ++i) put32(o, 0); put32(o, 0); put16(o, 3); put16(o, 0); put16(o, 0); o->b[o->n++] = 0; o->b[o->n++] = 0; close_chunk(o, c);
    close_chunk(o, g);
}
static void placed_tests(const char *dir)
{
    char root[400], folder[520], data[520], path[900], line[1000];
    WWhy why;
    WErr e;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-placed", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the placed models' fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ the fixtures: the placeholders' own bytes (a box, a checker),
     * a map's folder with a WC3 model, a game model, half a model, a
     * building, a green picture; an archive with the game model alone */
    Resolver *bare = NULL;
    ResolverPlace none = { .distinctness = 0.5 };
    char *quiet = NULL;
    size_t quiet_len = 0;
    none.log = open_memstream(&quiet, &quiet_len);
    e = resolver_open(&none, &bare, &why);
    check(!e, e ? why.text : "a resolver with no places opens");
    if (e) { fclose(none.log); free(quiet); return; }
    AssetAnswer box, box_skin, checker;
    WErr e1 = asset_stand_in(bare, "World\\Made\\Rock.m2", ASSET_MODEL, &box, &why);
    WErr e2 = asset_stand_in(bare, "World\\Made\\Rock00.skin", ASSET_MODEL, &box_skin, &why);
    WErr e3 = asset_stand_in(bare, "Textures\\Green.blp", ASSET_TEXTURE, &checker, &why);
    fflush(none.log);
    AssetCounts counts;
    asset_counts(bare, &counts);
    check(!e1 && !e2 && !e3 && box.source == ASSET_FROM_PLACEHOLDER && box_skin.len != box.len && checker.len > 1172 && counts.placeholder_answers == 3
          && quiet && strstr(quiet, "the model World\\Made\\Rock.m2: a box stands in for it, with the rest of its files"),
          "asset_stand_in: the placeholder alone, said as standing in whole and counted (a box, the box's skin, the checker)");
    AssetAnswer nothing;
    check(asset_get(NULL, NULL, "Textures\\Green.blp", ASSET_TEXTURE, &nothing, &why) == W_ERR_NOT_FOUND,
          "asset_get with no resolver and no archives: not found, no placeholder");
    unsigned char *green = malloc(checker.len);
    memcpy(green, checker.data, checker.len);
    for (size_t i = 1172; i + 4 <= checker.len; i += 4) { green[i] = 0; green[i + 1] = 255; green[i + 2] = 0; green[i + 3] = 255; }   /* blue, green, red, alpha */
    snprintf(folder, sizeof folder, "%s/map-build", root);
    static Bytes mdx, wmo_root, wmo_group;
    made_mdx(&mdx);
    t_made_wmo_root(&wmo_root);
    t_made_wmo_group(&wmo_group);
    t_write(folder, "war3mapImported/Made.mdx", mdx.b, mdx.n);
    t_write(folder, "Textures/Made.blp", green, checker.len);
    t_write(folder, "Textures/Hut.blp", green, checker.len);
    t_write(folder, "Tileset/Made/Grass.blp", green, checker.len);
    t_write(folder, "World/Made/Rock.m2", box.data, box.len);
    t_write(folder, "World/Made/Rock00.skin", box_skin.data, box_skin.len);
    t_write(folder, "World/Made/Half.m2", box.data, box.len);      /* its view isn't anywhere */
    t_write(folder, "World/Made/Hut.wmo", wmo_root.b, wmo_root.n);
    t_write(folder, "World/Made/Hut_000.wmo", wmo_group.b, wmo_group.n);
    snprintf(data, sizeof data, "%s/Data", root);
    t_mkdirs(data);
    snprintf(path, sizeof path, "%s/patch.MPQ", data);
    const char *const anames[] = { "World\\Made\\Rock.m2", "World\\Made\\Rock00.skin" };
    const char *const abytes[] = { (const char *)box.data, (const char *)box_skin.data };
    const size_t alens[] = { box.len, box_skin.len };
    t_write_mpq_n(path, anames, abytes, alens, 2);
    MpqChain chain;
    e = mpq_chain_open(&chain, data, "enUS", &why);
    int have_chain = !e;
    check(have_chain, e ? why.text : "an archive made here, with the game model alone, opens");
    /* }}} */

    ResolverPlace place = { .noverlays = 1, .distinctness = 0.5 };
    snprintf(place.overlays[0], sizeof place.overlays[0], "%s", folder);
    char *log = NULL;
    size_t log_len = 0;
    place.log = open_memstream(&log, &log_len);
    Resolver *r = NULL;
    e = resolver_open(&place, &r, &why);
    check(!e, e ? why.text : "the resolver opens the map's folder as its overlay, with no archives");
    PlacedModel pm;
    if (!e) {
        /* {{{ a WC3 model the map brought */
        e = placed_model_read(r, NULL, "war3mapImported\\Made.mdx", 0.5, &pm, &why);
        const M2Model *d = e ? NULL : placed_model_drawn(&pm);
        snprintf(line, sizeof line, "a placed .mdx the map's folder has is a WC3 model (MDX) from the folder, its points in yards by the map's rule (WC3's (5, 0, 5) at 0.5: (%.2f, %.2f, %.2f))",
                 d ? d->vertices[5].x : 0, d ? d->vertices[5].y : 0, d ? d->vertices[5].z : 0);
        check(!e && pm.wc3 && pm.source == ASSET_FROM_OVERLAY && !pm.m2_bytes && d == &pm.mdx.model && d->nvertices == 10
              && near(d->vertices[5].x, 2.5, 1e-6) && near(d->vertices[5].z, 2.5, 1e-6) && !strcmp(pm.base, "war3mapImported\\Made"), e ? why.text : line);
        if (!e) {
            AssetAnswer a;
            WErr ep = asset_get(r, NULL, d->textures[1].file, ASSET_TEXTURE, &a, &why);
            BlpImage img;
            int ok = !ep && a.source == ASSET_FROM_OVERLAY && !blp_decode(a.data, a.len, &img, &why);
            check(ok && img.rgba[1] == 255 && img.rgba[0] == 0, "its picture (Textures\\Made.blp) from the folder too, decoded (green)");
            if (ok) blp_free(&img);
            if (!ep) asset_answer_free(&a);
            check(d->textures[0].kind == M2_TEXTURE_WC3 + 1 && d->units[2].alpha == 0,
                  "its team colour is WC3's replaceable picture 1 (left off: 404) and its hidden layer has alpha 0 at rest (not drawn)");
            placed_model_free(&pm);
        }
        e = placed_model_read(r, NULL, "war3mapImported\\Made.mdx", 0, &pm, &why);
        check(!e && near(placed_model_drawn(&pm)->vertices[5].x, 5 / 30.72, 1e-6), "with no rule given, WC3's 30.72 units a yard");
        if (!e) placed_model_free(&pm);
        /* }}} */
        /* {{{ the game's models */
        e = placed_model_read(r, NULL, "World\\Made\\Rock.mdx", 0.5, &pm, &why);
        check(!e && !pm.wc3 && pm.source == ASSET_FROM_OVERLAY && pm.m2_bytes && pm.m2.nvertices == 8 && pm.m2.nunits == 1,
              e ? why.text : "a game model's .mdx name: its .m2 and 00.skin beside it, from the first place that has them (the folder)");
        if (!e) placed_model_free(&pm);
        fflush(place.log);
        size_t before = log_len;
        e = placed_model_read(r, NULL, "World\\Made\\Gone.m2", 0.5, &pm, &why);
        fflush(place.log);
        check(!e && pm.source == ASSET_FROM_PLACEHOLDER && pm.m2.nvertices == 8 && pm.m2_bytes
              && strstr(log + before, "the model World\\Made\\Gone.m2 is in none of 0 packs, 0 server folders, 1 overlay, no archives open; a box stands in")
              && strstr(log + before, "World\\Made\\Gone00.skin: the box's skin stands in for it"),
              e ? why.text : "a model no place has: the box stands in, its .m2 and its skin, said");
        if (!e) placed_model_free(&pm);
        before = log_len;
        e = placed_model_read(r, NULL, "World\\Made\\Half.m2", 0.5, &pm, &why);
        fflush(place.log);
        check(!e && pm.source == ASSET_FROM_PLACEHOLDER && strstr(log + before, "World\\Made\\Half00.skin is in none of")
              && strstr(log + before, "the model World\\Made\\Half.m2: a box stands in for it, with the rest of its files"),
              e ? why.text : "a model whose .m2 is there but not its view: the box stands in whole, not the .m2 with the box's view");
        if (!e) placed_model_free(&pm);
        check(placed_model_read(r, NULL, "", 0.5, &pm, &why) == W_ERR_REFUSED, "an empty name: refused");
        /* }}} */
        /* {{{ buildings */
        WmoRoot wr;
        AssetSource src = ASSET_SOURCE_COUNT;
        e = placed_building_root(r, NULL, "World\\Made\\Hut.wmo", &wr, &src, &why);
        check(!e && src == ASSET_FROM_OVERLAY && wr.ngroups == 1 && wr.nmaterials == 1 && !strcmp(wr.materials[0].picture, "Textures\\Hut.blp"),
              e ? why.text : "a building's root from the folder: one part, one material naming its picture");
        if (!e) wmo_free_root(&wr);
        WmoGroup wg;
        e = placed_building_group(r, NULL, "World\\Made\\Hut.wmo", 0, &wg, &why);
        check(!e && wg.nvertices == 3 && wg.nbatches == 1 && wg.batches[0].count == 3, e ? why.text : "its part _000 from the folder: one triangle, one batch");
        if (!e) wmo_free_group(&wg);
        check(placed_building_group(r, NULL, "World\\Made\\Hut.wmo", 1, &wg, &why) == W_ERR_NOT_FOUND, "a part it hasn't: not found");
        before = log_len;
        asset_counts(r, &counts);
        uint32_t stood = counts.placeholder_answers;
        e = placed_building_root(r, NULL, "World\\Made\\Shed.wmo", &wr, NULL, &why);
        fflush(place.log);
        asset_counts(r, &counts);
        check(e == W_ERR_NOT_FOUND && log_len == before && counts.placeholder_answers == stood, "a building no place has: not found, quietly (the box is no building's root)");
        e = placed_model_stand_in(r, "World\\Made\\Shed.wmo", &pm, &why);
        check(!e && pm.source == ASSET_FROM_PLACEHOLDER && pm.m2.nvertices == 8, e ? why.text : "the drawer stands the box in for it (placed_model_stand_in)");
        if (!e) placed_model_free(&pm);
        check(placed_model_stand_in(NULL, "World\\Made\\Shed.wmo", &pm, &why) == W_ERR_NOT_FOUND, "with no resolver no box stands in");
        /* }}} */
        /* {{{ ground pictures (033): the folder's, else the checker */
        GroundPlace gp = { .assets = r };
        GroundPixels px = { 0 };
        e = ground_prep_pixels(&gp, "Tileset\\Made\\Grass.blp", &px, &why);
        check(!e && px.width == 8 && px.rgba[1] == 255 && px.rgba[0] == 0 && px.smaller, e ? why.text : "a ground picture the map's folder has: its own (green), with its smaller copies");
        free(px.rgba); free(px.smaller);
        memset(&px, 0, sizeof px);
        e = ground_prep_pixels(&gp, "Tileset\\Made\\Sand.blp", &px, &why);
        check(!e && px.width == 8 && px.rgba[0] == 255 && px.rgba[1] == 0 && px.rgba[2] == 255, e ? why.text : "one no place has: the magenta checker stands in");
        free(px.rgba); free(px.smaller);
        /* }}} */
        resolver_free(r);
    }
    fclose(place.log);
    free(log);

    /* {{{ strict mode: no box */
    place.strict = 1;
    log = NULL;
    place.log = open_memstream(&log, &log_len);
    if (!resolver_open(&place, &r, &why)) {
        check(placed_model_read(r, NULL, "World\\Made\\Gone.m2", 0.5, &pm, &why) == W_ERR_NOT_FOUND && resolver_exit_status(r) == 1,
              "strict mode: a model no place has is refused, not a box, and the exit status says so");
        resolver_free(r);
    }
    fclose(place.log);
    free(log);
    /* }}} */

    /* {{{ no resolver (every run but a test map's): the archives alone, as before */
    if (have_chain) {
        e = placed_model_read(NULL, &chain, "World\\Made\\Rock.mdx", 0.5, &pm, &why);
        check(!e && !pm.wc3 && pm.source == ASSET_FROM_BLIZZARD && pm.m2.nvertices == 8, e ? why.text : "no resolver: a game model from the archives");
        if (!e) placed_model_free(&pm);
        e = placed_model_read(NULL, &chain, "World\\Made\\Gone.m2", 0.5, &pm, &why);
        check(e == W_ERR_NOT_FOUND && strstr(why.text, "World\\Made\\Gone.m2"), "no resolver: a model the archives lack is not found, named (left out, no box)");
        check(placed_model_read(NULL, &chain, "war3mapImported\\Made.mdx", 0.5, &pm, &why) == W_ERR_NOT_FOUND, "nor is a map's own model, with no map folder to find it in");
        GroundPlace gp = { .chain = &chain };
        GroundPixels px = { 0 };
        check(ground_prep_pixels(&gp, "Tileset\\Made\\Grass.blp", &px, &why) == W_ERR_NOT_FOUND, "no resolver: a ground picture the archives lack is not found (no checker)");
        mpq_chain_close(&chain);
    }
    /* }}} */
    asset_answer_free(&box); asset_answer_free(&box_skin); asset_answer_free(&checker);
    free(green);
    resolver_free(bare);
    fclose(none.log);
    free(quiet);
    t_remove_tree(root);
}
/* }}} */

/* {{{ Warcraft III's own install and a converted map's WC3 ground (1109
 * step 1d): the install as a resolver place (015, 060), its tileset sheet
 * (077), the map's ground painted with its tilesets (078, 033). Files and
 * archives made here; the converter's real output too when the build
 * folder carries the map's war3map.w3e. */

/* a BLP2 of plain BGRA pixels, w x h, one level, from RGBA */
static unsigned char *t_blp2(int w, int h, const unsigned char *rgba, size_t *len)
{
    size_t at = 148 + 1024, n = (size_t)w * h;
    *len = at + n * 4;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "BLP2", 4);
    t_le32(s + 4, 1);
    s[8] = 3; s[9] = 8;
    t_le32(s + 12, (uint32_t)w); t_le32(s + 16, (uint32_t)h);
    t_le32(s + 20, (uint32_t)at); t_le32(s + 84, (uint32_t)(n * 4));
    for (size_t i = 0; i < n; ++i) {
        s[at + i * 4] = rgba[i * 4 + 2]; s[at + i * 4 + 1] = rgba[i * 4 + 1];
        s[at + i * 4 + 2] = rgba[i * 4]; s[at + i * 4 + 3] = rgba[i * 4 + 3];
    }
    return s;
}
/* a made tileset's colour for subtile s (0 to 31) of tileset k (0 to
 * 31): each pair its own */
static void t_colour(int k, int s, unsigned char out[3])
{
    out[0] = (unsigned char)(20 + 7 * k);
    out[1] = (unsigned char)(4 + 8 * s);
    out[2] = (unsigned char)(230 - 5 * k);
}
/* a tileset picture as WC3 lays them out: 4 x 4 subtiles of 64 (8 x 4
 * when extended, the right half whole variations), each its own colour
 * (t_colour), subtile s's alpha full on exactly the quarters of its
 * corner bits (south-east 1, south-west 2, north-east 4, north-west 8;
 * north at the top) */
static unsigned char *t_tileset(int k, int extended, size_t *len)
{
    int w = extended ? 512 : 256, h = 256;
    unsigned char *rgba = malloc((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char *p = rgba + ((size_t)y * w + x) * 4;
            int s = (y / 64) * 4 + (x % 256) / 64, sx = x % 64, sy = y % 64;
            t_colour(k, x >= 256 ? 16 + s : s, p);
            int bit = sy < 32 ? (sx < 32 ? 8 : 4) : (sx < 32 ? 2 : 1);
            p[3] = (x >= 256 || (s & bit)) ? 255 : 0;
        }
    unsigned char *b = t_blp2(w, h, rgba, len);
    free(rgba);
    return b;
}
/* the tileset a cliff cell with a model is laid with (its cliff
 * tileset's ground, found by its id), or -1: no cliffs, no cliff, or no ground */
static int t_cliff_set(const Wc3Ground *w, int i, int j)
{
    const Wc3Cliffs *cl = wc3_ground_cliffs(w);
    int at = cl ? wc3_cliffs_at(cl, i, j) : -1;
    if (at < 0 || wc3_cliffs_get(cl, at)->ramp) return -1;   /* a ramp's cell keeps its ground */
    const char *ground = wc3_cliffs_type(cl, wc3_cliffs_get(cl, at)->type)->ground;
    for (int k = 0; ground[0] && k < wc3_ground_tilesets(w); ++k) if (!strcmp(wc3_ground_tileset(w, k)->id, ground)) return k;
    return -1;
}
/* WC3's own laying of cell (i, j), worked out here from its points:
 * its tilesets lowest first and the piece (0 to 31) each is laid with:
 * the lowest its south-west point's variation's (063's w3e_variation),
 * each higher one its corners' bits (south-east 1, south-west 2,
 * north-east 4, north-west 8). A cliff cell with a model: its cliff's
 * ground alone; a blighted cell (the blight named): the blight on its
 * blighted corners; another bare cell: its south-west point's tileset
 * alone. How many. */
static int t_wc3_cell(const Wc3Ground *w, const W3ePrep *prep, int i, int j, int t[4], int piece[4])
{
    const W3eGround *g = wc3_ground_w3e(w);
    int kind = prep->cell_kind[(size_t)j * (g->width - 1) + i], blight = wc3_ground_blight(w);
    const W3ePoint *sw = w3e_point(g, i, j);
    int corner[4] = { -1, -1, -1, -1 }, n = 0;            /* south-west, south-east, north-west, north-east */
    static const int BIT[4] = { 2, 1, 8, 4 };
    int cs = t_cliff_set(w, i, j);
    if (cs >= 0) { t[0] = cs; n = 1; }
    else if (kind == W3E_CELL_TEXTURED || (kind == W3E_CELL_BLIGHT && blight >= 0)) {
        for (int q = 0; q < 4; ++q) {
            const W3ePoint *p = w3e_point(g, i + (q & 1), j + (q >> 1));
            corner[q] = p->is_blight && blight >= 0 ? blight : p->ground_texture;
        }
        for (int q = 0; q < 4; ++q) {
            int seen = 0;
            for (int k = 0; k < n; ++k) seen |= t[k] == corner[q];
            if (!seen) t[n++] = corner[q];
        }
        for (int a = 1; a < n; ++a) for (int b = a; b > 0 && t[b - 1] > t[b]; --b) { int x = t[b]; t[b] = t[b - 1]; t[b - 1] = x; }
    } else { t[0] = sw->ground_texture; n = 1; }
    for (int k = 0; k < n; ++k) {
        piece[k] = 0;
        if (!k) piece[k] = w3e_variation(wc3_ground_tileset(w, t[0])->extended, sw->texture_details);
        else for (int q = 0; q < 4; ++q) if (corner[q] == t[k]) piece[k] |= BIT[q];
    }
    return n;
}
/* Compares a laid tile (078's pieces, the pictures they name as 033
 * unpacks them) with WC3's own laying, chunk by painted chunk: each
 * cell's pieces against t_wc3_cell's, and each blend point's colour, as
 * 080's shader works it out from the cells (a subtile's texel a half
 * texel in from its edges at most, the lowest whole and each higher one
 * over it by its alpha), against WC3's: with these pictures, WC3's
 * layering gives each quarter of a cell its corner's tileset's piece
 * (the lowest's variation, a higher one's corner piece). Counts all, and
 * apart those in chunks laying more than four tilesets. `px` holds each
 * of the tile's pictures unpacked. */
typedef struct { long points, same, many_points, many_same, cells, cells_same, varied, right; int chunks, many_chunks, hit_twice; double worst_centre; } TLayCheck;
static void t_lay_check(const Wc3Ground *w, const W3ePrep *prep, const AdtTile *adt, const Wc3Lay *lay, const unsigned char painted[256],
                        const GroundPixels *px, unsigned char *hit, TLayCheck *lc)
{
    const W3eGround *g = wc3_ground_w3e(w);
    const float step = ADT_CHUNK_SIZE / 8;
    /* each of the tile's pictures: its tileset, and 0 or 16 (an extended one's right half) */
    int pic_set[256], pic_off[256];
    for (int t = 0; t < adt->ntextures; ++t) {
        pic_set[t] = -1; pic_off[t] = 0;
        for (int k = 0; k < wc3_ground_tilesets(w); ++k) {
            const Wc3Tileset *ts = wc3_ground_tileset(w, k);
            if (!strcmp(adt->textures[t], ts->layer)) pic_set[t] = k;
            else if (ts->layer_right[0] && !strcmp(adt->textures[t], ts->layer_right)) { pic_set[t] = k; pic_off[t] = 16; }
        }
    }
    for (int k = 0; k < 256; ++k) {
        if (!painted[k]) continue;
        ++lc->chunks;
        const AdtChunk *c = &adt->chunks[k];
        int crow = k / 16, ccol = k % 16;
        int ci[8][8], cj[8][8], wt[8][8][4], wp[8][8][4], wn[8][8], used[32] = { 0 }, nused = 0;
        for (int r = 0; r < 8; ++r)
            for (int cc = 0; cc < 8; ++cc) {
                double fx, fy;
                wn[r][cc] = 0;
                if (!wc3_ground_cell_at(w, c->x - (r + 0.5) * step, c->y - (cc + 0.5) * step, &ci[r][cc], &cj[r][cc], &fx, &fy)) { ci[r][cc] = -1; continue; }
                double off = fabs(fx - 0.5) > fabs(fy - 0.5) ? fabs(fx - 0.5) : fabs(fy - 0.5);
                if (off > lc->worst_centre) lc->worst_centre = off;
                size_t at = (size_t)cj[r][cc] * (g->width - 1) + ci[r][cc];
                if (hit) { if (hit[at]) ++lc->hit_twice; hit[at] = 1; }
                wn[r][cc] = t_wc3_cell(w, prep, ci[r][cc], cj[r][cc], wt[r][cc], wp[r][cc]);
                for (int q = 0; q < wn[r][cc]; ++q) if (wt[r][cc][q] < 32 && !used[wt[r][cc][q]]) { used[wt[r][cc][q]] = 1; ++nused; }
                /* the cell's pieces as laid, against WC3's */
                const Wc3Lay *e = &lay[(size_t)(crow * 8 + r) * WC3_TILE_CELLS + (size_t)(ccol * 8 + cc)];
                int same = 1, q = 0;
                for (; q < 4 && e->sub[q] != WC3_PIECE_NONE; ++q)
                    same &= q < wn[r][cc] && pic_set[e->pic[q]] == wt[r][cc][q] && pic_off[e->pic[q]] + e->sub[q] == wp[r][cc][q];
                same &= q == wn[r][cc];
                ++lc->cells; lc->cells_same += same;
                lc->varied += wn[r][cc] && wp[r][cc][0] != 15;     /* its lowest not subtile 15 */
                lc->right += wn[r][cc] && wp[r][cc][0] >= 16;      /* one of an extended picture's whole variations */
            }
        int many = nused > 4;
        lc->many_chunks += many;
        for (int py = 0; py < 64; ++py)
            for (int pxl = 0; pxl < 64; ++pxl) {
                double rf = py * 8.0 / 63.0, cf = pxl * 8.0 / 63.0;
                int r = rf >= 8 ? 7 : (int)rf, cc = cf >= 8 ? 7 : (int)cf;
                if (ci[r][cc] < 0 || !wn[r][cc]) continue;
                double f[2] = { cf - cc, rf - r };
                /* the shader's colour */
                const Wc3Lay *e = &lay[(size_t)(crow * 8 + r) * WC3_TILE_CELLS + (size_t)(ccol * 8 + cc)];
                double got[3] = { 0, 0, 0 };
                for (int q = 0; q < 4 && e->sub[q] != WC3_PIECE_NONE; ++q) {
                    const GroundPixels *pp = &px[e->pic[q]];
                    int side = pp->width, sub = side / 4;
                    double gx = f[0] < 1.0 / 128 ? 1.0 / 128 : f[0] > 127.0 / 128 ? 127.0 / 128 : f[0];
                    double gy = f[1] < 1.0 / 128 ? 1.0 / 128 : f[1] > 127.0 / 128 ? 127.0 / 128 : f[1];
                    int X = (e->sub[q] % 4) * sub + (int)(gx * sub), Y = (e->sub[q] / 4) * sub + (int)(gy * sub);
                    const unsigned char *s = pp->rgba + ((size_t)Y * side + X) * 4;
                    double a = q ? s[3] / 255.0 : 1;
                    for (int d = 0; d < 3; ++d) got[d] += (s[d] - got[d]) * a;
                }
                /* WC3's: the nearest corner's tileset's piece */
                int east = f[0] >= 0.5, north = f[1] < 0.5;
                int t = wt[r][cc][0], pc = wp[r][cc][0];
                if (wn[r][cc] > 1) {
                    const W3ePoint *qp = w3e_point(g, ci[r][cc] + east, cj[r][cc] + north);
                    int corner = qp->is_blight && wc3_ground_blight(w) >= 0 ? wc3_ground_blight(w) : qp->ground_texture;
                    for (int q = 0; q < wn[r][cc]; ++q) if (wt[r][cc][q] == corner) { t = corner; pc = wp[r][cc][q]; }
                }
                unsigned char want[3];
                t_colour(t, pc, want);
                int same = fabs(got[0] - want[0]) <= 1 && fabs(got[1] - want[1]) <= 1 && fabs(got[2] - want[2]) <= 1;
                ++lc->points; lc->same += same;
                if (many) { ++lc->many_points; lc->many_same += same; }
            }
    }
}
/* each of a laid tile's pictures unpacked as 033 does (into px[256]) */
static void t_lay_pixels(const GroundPlace *gp, const AdtTile *adt, const Wc3Lay *lay, GroundPixels *px)
{
    WWhy why;
    memset(px, 0, sizeof *px * 256);
    unsigned char needed[256] = { 0 };
    for (size_t c = 0; c < (size_t)WC3_TILE_CELLS * WC3_TILE_CELLS; ++c)
        for (int q = 0; q < 4 && lay[c].sub[q] != WC3_PIECE_NONE; ++q) needed[lay[c].pic[q]] = 1;
    for (int t = 0; t < adt->ntextures; ++t) if (needed[t]) ground_prep_pixels(gp, adt->textures[t], &px[t], &why);
}
static void t_lay_pixels_free(GroundPixels *px) { for (int t = 0; t < 256; ++t) { free(px[t].rgba); free(px[t].smaller); } }

static void wc3_place_tests(const char *dir, const char *build)
{
    char root[400], folder[520], wc3[520], path[900], line[1000];
    WWhy why;
    WErr e;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-wc3", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the WC3 install's fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ the tileset sheet (077) */
    {
        static const char SHEET[] =
            "ID;PWXL;N;E\r\nB;X4;Y5;D0\r\nC;X1;Y1;K\"tileID\"\r\nC;X2;K\"dir\"\r\nC;X3;K\"file\"\r\nC;X4;K\"note\"\r\n"
            "C;X1;Y2;K\"Ldrt\"\r\nC;X2;K\"TerrainArt\\LordaeronSummer\"\r\nC;X3;K\"Lords_Dirt\"\r\nC;X4;K\"a;;b\"\r\n"
            "F;P0;FG0G;X2\r\nC;Y3;X1;K\"Lgrs\"\r\nC;X3;K\"Lords_Grass\"\r\nC;X4;K42\r\n"
            "C;X1;Y4;K\"Ldrt\"\r\nC;X3;K\"Lords_Dirt2\"\r\nE\r\nC;X1;Y5;K\"Gone\"\r\n";
        Slk *k = NULL;
        e = slk_read((const unsigned char *)SHEET, strlen(SHEET), &k, &why);
        check(!e, e ? why.text : "a SYLK sheet reads (CR LF lines, cells that leave their row or column out, other records passed over)");
        if (!e) {
            const char *d = slk_value(k, "Ldrt", "dir"), *f = slk_value(k, "Ldrt", "file"), *n = slk_value(k, "Ldrt", "note");
            check(d && !strcmp(d, "TerrainArt\\LordaeronSummer") && f && !strcmp(f, "Lords_Dirt2") && n && !strcmp(n, "a;b"),
                  "a row named twice is one row: the later one's cells win, the earlier one's others stay; \";;\" is one \";\"");
            check(slk_value(k, "Lgrs", "file") && !strcmp(slk_value(k, "Lgrs", "file"), "Lords_Grass") && !strcmp(slk_value(k, "Lgrs", "note"), "42")
                  && !slk_value(k, "Lgrs", "dir"), "fields in any order; an unquoted value kept as written; a cell it hasn't: none");
            check(slk_rows(k) == 2 && !strcmp(slk_row_id(k, 0), "Ldrt") && !strcmp(slk_row_id(k, 1), "Lgrs") && !slk_row_id(k, 2)
                  && !slk_value(k, "Gone", "file") && !slk_value(k, "Ldrt", "nothing"), "its rows once each, in order; nothing read after E; a column it hasn't: none");
            slk_free(k);
        }
        Slk *bad = NULL;
        check(slk_read((const unsigned char *)"C;X1;Y1;K\"a\"\n", 13, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "ID;"), "no ID; head: corrupt");
        check(slk_read((const unsigned char *)"ID;P\nC;X0;Y1;K1\n", 16, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "line 2"), "a column 0: corrupt, naming the line");
        check(slk_read((const unsigned char *)"ID;P\nC;X2;Y1;K1\n", 16, &bad, &why) == W_ERR_CORRUPT && strstr(why.text, "column 1"), "no row 1 naming column 1: corrupt");
    }
    /* }}} */

    /* {{{ the install's archives (015): found without regard to capitals, newest first */
    snprintf(wc3, sizeof wc3, "%s/wc3", root);
    t_mkdirs(wc3);
    {
        const char *names[] = { "Textures\\Both.blp", "Textures\\Only.blp", "Textures\\Base.blp" };
        const char *base[] = { "war3's", "war3's only", "war3's base" };
        snprintf(path, sizeof path, "%s/WAR3.MPQ", wc3);
        t_write_mpq(path, names, base, 3);
        const char *patched[] = { "the patch's" };
        snprintf(path, sizeof path, "%s/war3patch.mpq", wc3);
        t_write_mpq(path, names + 1, patched, 1);
        MpqChain ch;
        e = mpq_chain_open_wc3(&ch, wc3, &why);
        check(!e && ch.count == 2, e ? why.text : "WC3's archives open from a folder, found without regard to capitals (WAR3.MPQ, war3patch.mpq)");
        if (!e) {
            size_t n = 0;
            const char *from = NULL;
            unsigned char *s = mpq_chain_read(&ch, "Textures\\Only.blp", &n, &from);
            check(s && n == 11 && !memcmp(s, "the patch's", 11) && strstr(from, "war3patch.mpq"), "the patch first: its copy of a file both have");
            free(s);
            s = mpq_chain_read(&ch, "textures/base.BLP", &n, &from);
            check(s && n == 11 && strstr(from, "WAR3.MPQ"), "the base game for the rest");
            free(s);
            mpq_chain_close(&ch);
        }
        snprintf(path, sizeof path, "%s/empty", root);
        t_mkdirs(path);
        check(mpq_chain_open_wc3(&ch, path, &why) == W_ERR_NOT_FOUND && strstr(why.text, "war3.mpq"), "a folder with none of them: not found, naming them");
        snprintf(path, sizeof path, "%s/nowhere", root);
        check(mpq_chain_open_wc3(&ch, path, &why) == W_ERR_NOT_FOUND && strstr(why.text, "isn't there"), "no folder: not found");
    }
    /* }}} */

    /* {{{ the install as a place (060): after the game's archives, before the placeholder */
    {
        char data[520];
        snprintf(data, sizeof data, "%s/Data", root);
        t_mkdirs(data);
        const char *names[] = { "Textures\\Both.blp" };
        const char *wow[] = { "the game's" };
        snprintf(path, sizeof path, "%s/common.MPQ", data);
        t_write_mpq(path, names, wow, 1);
        MpqChain game;
        e = mpq_chain_open(&game, data, "enUS", &why);
        check(!e, e ? why.text : "a made archive of the game's opens");
        Settings *st = settings_new();
        ResolverPlace place;
        e = resolver_place_from_settings(st, root, e ? NULL : &game, &place, &why);
        snprintf(line, sizeof line, "%s/wc3", root);
        check(!e && !strcmp(place.wc3, line) && !strcmp(settings_text(st, "assetWc3"), "wc3"), "the setting assetWc3 names assets/wc3 by default");
        ResolverPlace bare;
        check(!resolver_place_from_settings(st, "", NULL, &bare, &why) && !bare.wc3[0], "with no assets folder given, no install");
        check(settings_set(st, "assetWc3", "a,b", &why) == W_ERR_REFUSED && settings_set(st, "assetWc3", "..", &why) == W_ERR_REFUSED
              && settings_set(st, "assetWc3", "", &why) == W_OK && !resolver_place_from_settings(st, root, NULL, &bare, &why) && !bare.wc3[0],
              "one folder name, never a list or a way out; empty: none");
        settings_free(st);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        e = resolver_open(&place, &r, &why);
        fflush(place.log);
        check(!e && resolver_wc3(r) && !strcmp(resolver_wc3(r), line) && log && strstr(log, "Warcraft III's own archives from"), e ? why.text : "the resolver opens the install itself, and says so");
        if (!e) {
            AssetAnswer a;
            e = asset_resolve(r, "Textures\\Both.blp", ASSET_TEXTURE, &a, &why);
            check(!e && a.source == ASSET_FROM_BLIZZARD && a.len == 10 && strstr(a.from, "common.MPQ"), "a name both have: the game's own archives first");
            asset_answer_free(&a);
            e = asset_resolve(r, "Textures\\Only.blp", ASSET_TEXTURE, &a, &why);
            check(!e && a.source == ASSET_FROM_BLIZZARD && a.len == 11 && strstr(a.from, "war3patch.mpq"), "a name only WC3 has: from its archives, its patch first");
            asset_answer_free(&a);
            AssetAnswer all[4];
            int n = 0;
            check(!asset_resolve_all(r, "Textures\\Both.blp", ASSET_TEXTURE, all, 4, &n, &why) && n == 2 && strstr(all[1].from, "WAR3.MPQ"), "every copy: the game's, then WC3's");
            for (int i = 0; i < n; ++i) asset_answer_free(&all[i]);
            e = asset_resolve(r, "Textures\\Neither.blp", ASSET_TEXTURE, &a, &why);
            fflush(place.log);
            check(!e && a.source == ASSET_FROM_PLACEHOLDER && strstr(log, "the archives or Warcraft III's"), "a name neither has: the checker, the warning naming both");
            asset_answer_free(&a);
            resolver_free(r);
        }
        fclose(place.log); free(log);
        /* a folder named but not there: said, not a failure */
        snprintf(place.wc3, sizeof place.wc3, "%s/nowhere", root);
        log = NULL;
        place.log = open_memstream(&log, &log_len);
        e = resolver_open(&place, &r, &why);
        fflush(place.log);
        check(!e && !resolver_wc3(r) && strstr(log, "no Warcraft III install at"), e ? why.text : "an install folder that isn't there: said in the log, and left out");
        if (!e) resolver_free(r);
        fclose(place.log); free(log);
        /* an archive there that isn't one: the start stops, naming it */
        snprintf(folder, sizeof folder, "%s/broken", root);
        t_write_text(folder, "war3.mpq", "not an archive");
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        place.log = open_memstream(&log, &log_len);
        check(resolver_open(&place, &r, &why) != W_OK && strstr(why.text, "assetWc3"), "an archive that can't be read: refused, naming the setting");
        fclose(place.log); free(log);
        if (game.count) mpq_chain_close(&game);
    }
    /* }}} */

    /* {{{ a made map's WC3 ground painted with a made install's tilesets (078) */
    {
        /* the install: the sheet and five tileset pictures (Agrs extended),
         * Zzzz in the map's list but not in the sheet */
        static const char *IDS = "AdrtAgrsArckAvinAlshZzzz";
        char sheet[2000] = "ID;PWXL;N;E\nC;X1;Y1;K\"tileID\"\nC;X2;K\"dir\"\nC;X3;K\"file\"\n";
        const char *names[7];
        const char *bytes[7];
        size_t lens[7];
        char pnames[5][64];
        for (int k = 0; k < 5; ++k) {
            char row[200];
            snprintf(row, sizeof row, "C;X1;Y%d;K\"%.4s\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"%.4s_pic\"\n", k + 2, IDS + 4 * k, IDS + 4 * k);
            strncat(sheet, row, sizeof sheet - strlen(sheet) - 1);
            snprintf(pnames[k], sizeof pnames[k], "TerrainArt\\Made\\%.4s_pic.blp", IDS + 4 * k);
            names[k] = pnames[k];
            bytes[k] = (const char *)t_tileset(k, k == 1, &lens[k]);
        }
        strncat(sheet, "E\n", sizeof sheet - strlen(sheet) - 1);
        names[5] = "TerrainArt\\Terrain.slk"; bytes[5] = sheet; lens[5] = strlen(sheet);
        snprintf(folder, sizeof folder, "%s/install", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, names, bytes, lens, 6);
        for (int k = 0; k < 5; ++k) free((void *)bytes[k]);
        /* the map: 17 x 9 tile points (16 x 8 cells), its south-west corner
         * at WC3 (0, 0); points west of x 9 use tilesets 0 to 3, the rest 0
         * to 4; point (2, 2) blighted, point (5, 5) a cliff level up */
        W3eOut o = { 0 };
        w3e_head(&o, 11, 'A', IDS, "CAdi", 17, 9, 0.0f, 0.0f);
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 17; ++x) {
                int tex = x <= 8 ? (x * 3 + y * 5 + x * y) % 4 : (x + 2 * y) % 5;
                w3e_pt(&o, 8192, 8192, tex, x == 2 && y == 2 ? 0x20 : 0, (x + y) % 16, 0, x == 5 && y == 5 ? 3 : 2, 15);
            }
        char map[520];
        snprintf(map, sizeof map, "%s/map", root);
        t_write(map, "war3map.w3e", o.b, o.n);
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", map);
        place.noverlays = 1;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        e = resolver_open(&place, &r, &why);
        check(!e, e ? why.text : "a resolver with the map's folder and the made install opens");
        /* the rule: chunk 0 of the tile below holds WC3 cells 0-7 west to
         * east and 0-7 south to north, chunk 1 cells 8-15 */
        const double s = 1 / 30.72, X0 = 1000, Y0 = 2000, step = ADT_CHUNK_SIZE / 8;
        Wc3Ground *w = NULL;
        if (!e) e = wc3_ground_open(r, NULL, s, X0 - 8 * step, Y0, &w, &why);
        check(!e && wc3_ground_tilesets(w) == 6, e ? why.text : "the map's WC3 ground opens: its war3map.w3e from its folder, the sheet and the pictures from the install");
        if (!e) {
            const Wc3Tileset *t1 = wc3_ground_tileset(w, 1), *t5 = wc3_ground_tileset(w, 5);
            check(!strcmp(t1->id, "Agrs") && !strcmp(t1->path, "TerrainArt\\Made\\Agrs_pic.blp") && t1->source == ASSET_FROM_BLIZZARD && t1->extended
                  && !wc3_ground_tileset(w, 0)->extended && t1->width == 512 && !strcmp(t1->layer, "TerrainArt\\Made\\Agrs_pic.blp" WC3_LAYER_MARK)
                  && !strcmp(t1->layer_right, "TerrainArt\\Made\\Agrs_pic.blp" WC3_RIGHT_MARK WC3_LAYER_MARK) && !wc3_ground_tileset(w, 0)->layer_right[0],
                  "each tileset: its id, its picture's path from the sheet, from the install, extended when twice as wide (a layer for each half)");
            check(!strcmp(t5->id, "Zzzz") && !t5->path[0] && t5->source == ASSET_FROM_PLACEHOLDER, "a tileset the sheet has no row for: the checker");
            check(!wc3_ground_cliffs(w), "an install with no CliffTypes.slk: no cliffs, their cells laid as before");
            unsigned char *px = NULL;
            int side = 0;
            unsigned char want0[3], want16[3], want31[3];
            t_colour(1, 0, want0); t_colour(1, 16, want16); t_colour(1, 31, want31);
            e = wc3_ground_layer(w, t1->layer, &px, &side, &why);
            check(!e && side == 256 && !memcmp(px, want0, 3) && px[3] == 0 && px[(256 * 256 - 1) * 4 + 3] == 255,
                  "a layer's picture: its tileset's picture's left square, 4 x 4 subtiles of 64, alpha and all (subtile 0 clear, 15 whole)");
            free(px);
            e = wc3_ground_layer(w, t1->layer_right, &px, &side, &why);
            check(!e && side == 256 && !memcmp(px, want16, 3) && !memcmp(px + (256 * 256 - 1) * 4, want31, 3) && px[3] == 255,
                  "an extended picture's right half: its 16 whole variations, a layer of its own");
            free(px);
            check(wc3_ground_layer(w, "Textures\\Other.blp|wc3", &px, &side, &why) == W_ERR_NOT_FOUND, "a layer no tileset has: not found");
            int i, j;
            double fx, fy;
            check(wc3_ground_cell_at(w, X0 - 0.5 * step, Y0 - 0.5 * step, &i, &j, &fx, &fy) && i == 0 && j == 7 && near(fx, 0.5, 1e-6) && near(fy, 0.5, 1e-6)
                  && wc3_ground_cell_at(w, X0 - 7.5 * step, Y0 - 15.5 * step, &i, &j, NULL, NULL) && i == 15 && j == 0
                  && !wc3_ground_cell_at(w, X0 + 1, Y0 - 1, &i, &j, NULL, NULL) && !wc3_ground_cell_at(w, X0 - 1, Y0 - 16 * 8 * step - 1, &i, &j, NULL, NULL),
                  "a world place's WC3 cell by the map's rule: the chunk's north-west cell is WC3's (0, 7), its centre in the middle; past the grid, none");
            /* the tile: chunks laid out as a ground file's, two pictures of its own */
            AdtTile *adt = calloc(1, sizeof *adt);
            adt->textures = calloc(2, sizeof *adt->textures);
            snprintf(adt->textures[0], sizeof adt->textures[0], "Tileset\\Made\\Own0.blp");
            snprintf(adt->textures[1], sizeof adt->textures[1], "Tileset\\Made\\Own1.blp");
            adt->ntextures = 2;
            int tex[256][4];
            for (int k = 0; k < 256; ++k) {
                adt->chunks[k].x = (float)(X0 - (k / 16) * 8 * step);
                adt->chunks[k].y = (float)(Y0 - (k % 16) * 8 * step);
                for (int l = 0; l < 4; ++l) tex[k][l] = l % 2;
            }
            Wc3Lay *lay = malloc(sizeof *lay * WC3_TILE_CELLS * WC3_TILE_CELLS);
            unsigned char painted[256];
            Wc3Paint pc = { 0 };
            e = wc3_ground_lay_tile(w, adt, lay, tex, painted, &pc, &why);
            int others = 0, unlaid = 0;
            for (int k = 2; k < 256; ++k) others += painted[k] || tex[k][0] != 0 || tex[k][1] != 1;
            for (int r = 0; r < WC3_TILE_CELLS; ++r)
                for (int c = 0; c < WC3_TILE_CELLS; ++c) unlaid += (r < 8 && c < 16) != (lay[r * WC3_TILE_CELLS + c].sub[0] != WC3_PIECE_NONE);
            snprintf(line, sizeof line, "the two chunks on the WC3 grid laid (%d cells, %d bare, %d with another variation; one chunk lays %d tilesets), the other 254 left with the ground file's pictures, their cells with no piece",
                     pc.cells, pc.bare, pc.varied, pc.most);
            check(!e && painted[0] && painted[1] && !others && !unlaid && pc.painted == 2 && pc.cells == 128 && pc.outside == 0 && pc.bare == 8 && pc.most == 5 && pc.varied > 64, e ? why.text : line);
            int once = 1, named = 1;
            for (int t = 2; t < adt->ntextures; ++t) {
                int found = 0;
                for (int k = 0; k < wc3_ground_tilesets(w); ++k) found |= !strcmp(adt->textures[t], wc3_ground_tileset(w, k)->layer) || !strcmp(adt->textures[t], wc3_ground_tileset(w, k)->layer_right);
                named &= found;
                for (int u = t + 1; u < adt->ntextures; ++u) once &= strcmp(adt->textures[t], adt->textures[u]) != 0;
            }
            check(adt->ntextures > 2 + 4 && once && named && tex[0][0] == tex[0][3] && tex[0][0] >= 2,
                  "the tile's list gains a layer for each tileset picture (or half) its cells lay, once each; a chunk's own four its most used tileset's");
            /* 063's preparation, for which cells are bare */
            uint8_t ext[6] = { 0, 1, 0, 0, 0, 0 };
            W3eGround *g2 = NULL;
            W3ePrep *prep = NULL;
            if (!w3e_read(o.b, o.n, &g2, &why)) w3e_prep(g2, ext, 6, &prep, &why);
            GroundPlace lp = { .assets = r, .wc3 = w };
            GroundPixels *lpx = malloc(sizeof *lpx * 256);
            t_lay_pixels(&lp, adt, lay, lpx);
            TLayCheck chk = { 0 };
            if (prep) t_lay_check(w, prep, adt, lay, painted, lpx, NULL, &chk);
            snprintf(line, sizeof line, "both chunks as WC3 lays them, cell by cell: %ld of %ld cells' pieces, %ld of %ld points (each quarter of a cell its corner's tileset's piece, the lowest's its variation's; a bare cell its south-west one's)",
                     chk.cells_same, chk.cells, chk.same, chk.points);
            check(prep && chk.cells == 128 && chk.cells_same == 128 && chk.points == 8192 && chk.same == 8192, line);
            snprintf(line, sizeof line, "chunk 1, laying five tilesets, as WC3 lays it too (%ld of %ld points; four were the most before)", chk.many_same, chk.many_points);
            check(chk.many_chunks == 1 && chk.many_points == 4096 && chk.many_same == 4096, line);
            t_lay_pixels_free(lpx); free(lpx);
            /* the cells as the card takes them (033), each piece's picture by its layer there */
            GroundPrep *fake = calloc(1, sizeof *fake);
            fake->wc3_lay = lay;
            int layer_of[256];
            for (int t = 0; t < 256; ++t) layer_of[t] = 255 - t;
            unsigned char *texels = malloc((size_t)2 * WC3_TILE_CELLS * WC3_TILE_CELLS * 4);
            ground_prep_cell_texels(fake, layer_of, texels);
            int packed = 1;
            for (int r = 0; r < WC3_TILE_CELLS; ++r)
                for (int c = 0; c < WC3_TILE_CELLS; ++c)
                    for (int q = 0; q < 4; ++q) {
                        const Wc3Lay *e2 = &lay[r * WC3_TILE_CELLS + c];
                        const unsigned char *tx = texels + ((size_t)r * 256 + (size_t)c * 2) * 4;
                        packed &= tx[4 + q] == e2->sub[q] && tx[q] == (e2->sub[q] == WC3_PIECE_NONE ? 0 : layer_of[e2->pic[q]]);
                    }
            check(packed, "the cells packed for the card: a cell's row its row, texel 2c its pieces' layers there, 2c + 1 their subtiles (255 none)");
            check(strstr(GROUND_FS, "uniform sampler2D cells") && strstr(GROUND_FS, "uniform int has_cells") && strstr(GROUND_FS, "texelFetch(cells, ivec2(at.x * 2, at.y)")
                  && strstr(GROUND_FS, "texelFetch(cells, ivec2(at.x * 2 + 1, at.y)") && strstr(GROUND_VS, "flat out vec4 lay"),
                  "080's shader reads the cells as they are packed: texel 2c the layers, 2c + 1 the subtiles, by the uniforms 038 sets");
            free(texels); free(fake);
            free(lay);
            w3e_prep_free(prep); w3e_free(g2);
            /* through the ground's preparation (033): a WC3 layer's name is its square of subtiles */
            GroundPlace gp = { .assets = r, .wc3 = w };
            GroundPixels gpx = { 0 };
            e = ground_prep_pixels(&gp, t1->layer_right, &gpx, &why);
            check(!e && gpx.width == 256 && gpx.height == 256 && gpx.smaller && gpx.rgba[2] == want16[2], e ? why.text : "033 gives a WC3 layer's name its square of subtiles, with its smaller copies");
            free(gpx.rgba); free(gpx.smaller);
            gp.wc3 = NULL;
            check(ground_prep_pixels(&gp, t1->layer, &gpx, &why) == W_ERR_NOT_FOUND, "a WC3 layer with no WC3 ground: not found");
            adt_free(adt); free(adt);
            wc3_ground_free(w);
        }
        if (r) resolver_free(r);
        fclose(place.log); free(log);
        /* no install (no sheet anywhere), and no war3map.w3e: nothing painted, said */
        place.wc3[0] = '\0';
        log = NULL;
        place.log = open_memstream(&log, &log_len);
        if (!resolver_open(&place, &r, &why)) {
            check(wc3_ground_open(r, NULL, s, 0, 0, &w, &why) == W_ERR_NOT_FOUND && strstr(why.text, "assetWc3") && !w,
                  "no Warcraft III install: not found (no Terrain.slk), so the converter's pictures stay");
            resolver_free(r);
        }
        fclose(place.log); free(log);
        place.noverlays = 0;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        log = NULL;
        place.log = open_memstream(&log, &log_len);
        if (!resolver_open(&place, &r, &why)) {
            check(wc3_ground_open(r, NULL, s, 0, 0, &w, &why) == W_ERR_NOT_FOUND && strstr(why.text, "war3map.w3e") && strstr(why.text, "converter"),
                  "a build with no war3map.w3e: not found, saying which converter copies it");
            check(wc3_ground_open(NULL, NULL, s, 0, 0, &w, &why) == W_ERR_NOT_FOUND, "no resolver (not a test-map run): not found");
            resolver_free(r);
        }
        fclose(place.log); free(log);
        free(o.b);
    }
    /* }}} */

    /* {{{ the converter's real output, when its build carries the map's war3map.w3e */
    {
        snprintf(path, sizeof path, "%s/war3map.w3e", build);
        FILE *f = fopen(path, "rb");
        ConvertedMap m;
        if (!f || converted_map_read(build, &m, &why)) {
            if (f) fclose(f);
            printf("  (no war3map.w3e in %s: a build from before world-edit-to-execute's PR #246, which copies it; its WC3 tileset checks are skipped)\n", build);
            return;
        }
        fclose(f);
        printf("  the converted build folder %s, with its map's war3map.w3e\n", build);
        /* an install made for its own tilesets: the sheet, one picture each
         * in this test's colours (the pictures in two archives) */
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", build);
        place.noverlays = 1;
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        Wc3Ground *w = NULL;
        e = resolver_open(&place, &r, &why);
        if (!e) {
            check(wc3_ground_open(r, NULL, m.wc3_scale, m.wc3_x0, m.wc3_y0, &w, &why) == W_ERR_NOT_FOUND && strstr(why.text, "Terrain.slk"),
                  "with no Warcraft III install, its ground keeps the converter's pictures (said)");
            resolver_free(r);
            r = NULL;
        }
        AssetAnswer a;
        W3eGround *g = NULL;
        unsigned char *wb = NULL;
        size_t wl = 0;
        f = fopen(path, "rb");
        if (f) { fseek(f, 0, SEEK_END); wl = (size_t)ftell(f); fseek(f, 0, SEEK_SET); wb = malloc(wl); if (fread(wb, 1, wl, f) != wl) wl = 0; fclose(f); }
        e = w3e_read(wb, wl, &g, &why);
        check(!e, e ? why.text : "its war3map.w3e reads (063)");
        if (e) { fclose(place.log); free(log); free(wb); return; }
        int nset = g->nground < 16 ? g->nground : 16;
        char sheet[4000] = "ID;PWXL;N;E\nC;X1;Y1;K\"tileID\"\nC;X2;K\"dir\"\nC;X3;K\"file\"\n";
        const char *n1[16], *b1[16], *n2[16], *b2[16];
        size_t l1[16], l2[16];
        int c1 = 0, c2 = 0;
        static char pn[16][64];
        for (int k = 0; k < nset; ++k) {
            char row[200];
            snprintf(row, sizeof row, "C;X1;Y%d;K\"%.4s\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"T%02d\"\n", k + 2, g->ground_ids + 4 * k, k);
            strncat(sheet, row, sizeof sheet - strlen(sheet) - 1);
            snprintf(pn[k], sizeof pn[k], "TerrainArt\\Made\\T%02d.blp", k);
            if (k < 8) { n1[c1] = pn[k]; b1[c1] = (const char *)t_tileset(k, k % 3 == 1, &l1[c1]); ++c1; }
            else { n2[c2] = pn[k]; b2[c2] = (const char *)t_tileset(k, k % 3 == 1, &l2[c2]); ++c2; }
        }
        strncat(sheet, "E\n", sizeof sheet - strlen(sheet) - 1);
        n1[c1] = "TerrainArt\\Terrain.slk"; b1[c1] = sheet; l1[c1] = strlen(sheet); ++c1;
        snprintf(folder, sizeof folder, "%s/install-real", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, n1, b1, l1, c1);
        snprintf(path, sizeof path, "%s/War3x.mpq", folder);
        if (c2) t_write_mpq_n(path, n2, b2, l2, c2);
        for (int k = 0; k < c1 - 1; ++k) free((void *)b1[k]);
        for (int k = 0; k < c2; ++k) free((void *)b2[k]);
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        e = resolver_open(&place, &r, &why);
        if (!e) e = wc3_ground_open(r, NULL, m.wc3_scale, m.wc3_x0, m.wc3_y0, &w, &why);
        snprintf(line, sizeof line, "with an install made for its %d tilesets (main tileset %c), its WC3 ground opens: %d x %d tile points", nset, g->tileset_code, g->width, g->height);
        check(!e, e ? why.text : line);
        if (!e) {
            /* every ground tile of the build, prepared as 033 does with the WC3 ground set */
            GroundPlace gp;
            MapWorld *mw = calloc(1, sizeof *mw);
            maps_world_init(mw, root, m.id);                 /* no server tiles: every point lit as level ground */
            e = ground_place_init(&gp, NULL, r, m.directory, mw, 0, 0, &why);
            gp.wc3 = w;
            const W3eGround *wg = wc3_ground_w3e(w);
            unsigned char *hit = calloc((size_t)(wg->width - 1) * (wg->height - 1), 1);
            uint8_t ext[16];
            for (int k = 0; k < 16; ++k) ext[k] = k < wc3_ground_tilesets(w) && wc3_ground_tileset(w, k)->extended;
            W3ePrep *prep = NULL;
            w3e_prep(wg, ext, wc3_ground_tilesets(w), &prep, &why);
            TLayCheck chk = { 0 };
            int tiles = 0, own = 0, layers = 0, squares = 0, packed = 1, found_cell = 1;
            for (int a1 = 0; !e && a1 < 64; ++a1)
                for (int b1x = 0; !e && b1x < 64; ++b1x) {
                    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", m.directory, m.directory, a1, b1x);
                    if (asset_read(r, NULL, path, ASSET_GROUND, &a, &why)) continue;
                    asset_answer_free(&a);
                    GroundPrep *gpr = NULL;
                    e = ground_prep_tile(&gp, b1x, a1, NULL, NULL, &gpr, &why);
                    if (e) break;
                    ++tiles;
                    unsigned char painted[256] = { 0 };
                    for (int k = 0; k < 256; ++k) {
                        painted[k] = gpr->wc3_lay && gpr->wc3_lay[(size_t)(k / 16 * 8) * WC3_TILE_CELLS + (size_t)(k % 16 * 8)].sub[0] != WC3_PIECE_NONE;
                        own += !painted[k];
                    }
                    for (int t = 0; t < gpr->adt->ntextures; ++t)
                        if (gpr->needed[t] && strstr(gpr->adt->textures[t], WC3_LAYER_MARK)) { ++layers; squares += gpr->pixels[t].width == 256 && gpr->pixels[t].height == 256; }
                    if (gpr->wc3_lay) {
                        t_lay_check(w, prep, gpr->adt, gpr->wc3_lay, painted, gpr->pixels, hit, &chk);
                        /* packed for the card as laid; and each cell's centre point, by the shader's own reckoning
                         * from 033's texture places (its chunk from uv2, the cell in it from uv), finds its own */
                        int layer_of[256];
                        for (int t = 0; t < 256; ++t) layer_of[t] = t;
                        unsigned char *texels = malloc((size_t)2 * WC3_TILE_CELLS * WC3_TILE_CELLS * 4);
                        ground_prep_cell_texels(gpr, layer_of, texels);
                        for (size_t c = 0; c < (size_t)WC3_TILE_CELLS * WC3_TILE_CELLS; ++c)
                            for (int q = 0; q < 4; ++q)
                                packed &= texels[c * 8 + 4 + q] == gpr->wc3_lay[c].sub[q] && (gpr->wc3_lay[c].sub[q] == WC3_PIECE_NONE || texels[c * 8 + q] == gpr->wc3_lay[c].pic[q]);
                        free(texels);
                        for (int qn = 0; qn < 4; ++qn) {
                            const GroundQuarter *gq = &gpr->quarter[qn];
                            for (int qk = 0; qk < 64; ++qk)
                                for (int i = 0; i < 145; ++i) {
                                    if (i % 17 < 9) continue;           /* the cells' centres */
                                    int crow = (qn / 2) * 8 + qk / 8, ccol = (qn % 2) * 8 + qk % 8;
                                    const float *u = gq->texcoords + (qk * 145 + i) * 2, *u2 = gq->texcoords2 + (qk * 145 + i) * 2;
                                    int cx = (int)floorf(u[0]), cy = (int)floorf(u[1]);
                                    cx = cx < 0 ? 0 : cx > 7 ? 7 : cx; cy = cy < 0 ? 0 : cy > 7 ? 7 : cy;
                                    int ax = (int)floorf(u2[0] * 16) * 8 + cx, ay = (int)floorf(u2[1] * 16) * 8 + cy;
                                    found_cell &= ay == crow * 8 + i / 17 && ax == ccol * 8 + (i % 17 - 9);
                                }
                        }
                    }
                    ground_prep_free(gpr, 0);
                }
            long cells = 0;
            for (size_t c = 0; c < (size_t)(wg->width - 1) * (wg->height - 1); ++c) cells += hit[c];
            snprintf(line, sizeof line, "its %d ground tiles prepared with WC3's tilesets: %d chunks painted, %d keep the converter's pictures (off the WC3 grid)", tiles, chk.chunks, own);
            check(!e && tiles > 0 && chk.chunks > 0, e ? why.text : line);
            snprintf(line, sizeof line, "every WC3 cell (%ld of %d x %d) lies in exactly one cell of the ground's chunks, its centre on WC3's (%d twice; the farthest %.4f of a cell off)",
                     cells, wg->width - 1, wg->height - 1, chk.hit_twice, chk.worst_centre);
            check(cells == (long)(wg->width - 1) * (wg->height - 1) && chk.hit_twice == 0 && chk.worst_centre < 0.01, line);
            snprintf(line, sizeof line, "every cell laid as WC3 lays it, each its own variation (%ld of %ld cells' pieces; %ld laid with a piece other than subtile 15, %ld of them an extended picture's variation)",
                     chk.cells_same, chk.cells, chk.varied, chk.right);
            check(chk.cells > 0 && chk.cells_same == chk.cells, line);
            snprintf(line, sizeof line, "over all its chunks %ld of %ld blend points (%.1f%%) as WC3 lays them, as the shader reads the cells",
                     chk.same, chk.points, chk.points ? 100.0 * chk.same / chk.points : 0.0);
            check(chk.points > 0 && chk.same == chk.points, line);
            snprintf(line, sizeof line, "in its %d chunks laying more than four tilesets too: %ld of %ld blend points", chk.many_chunks, chk.many_same, chk.many_points);
            check(chk.many_same == chk.many_points, line);
            check(packed && found_cell, "its cells packed for the card as laid, and each cell's centre point finds its own cell by the shader's reckoning from 033's texture places");
            snprintf(line, sizeof line, "each WC3 layer a tile needs unpacked as its square of subtiles, 256 x 256 (%d of %d)", squares, layers);
            check(layers > 0 && squares == layers, line);
            free(hit);
            w3e_prep_free(prep);
            maps_world_free(mw); free(mw);
            wc3_ground_free(w);
        }
        if (r) resolver_free(r);
        fclose(place.log); free(log);
        w3e_free(g);
        free(wb);
    }
    /* }}} */
}
/* }}} */

/* {{{ 1109 step 1e: a converted map's cliffs as WC3's own cliff models (079) */
/* made_mdx with its first picture WC3's replaceable 11 (a cliff's), and
 * its first geoset's square moved to a cliff's cell: its corners where
 * WC3's cliff models have them, west of the origin (-128 to 0 east, 0
 * to 128 north), on the model's floor (step 1h: the bend lands them) */
static void t_cliff_mdx(Bytes *o)
{
    made_mdx(o);
    for (size_t i = 0; i + 12 <= o->n; ++i)
        if (!memcmp(o->b + i, "TEXS", 4)) { o->b[i + 8] = 11; break; }
    static const float corners[12] = { -128, 0, 0,   0, 0, 0,   0, 128, 0,   -128, 128, 0 };
    for (size_t i = 0; i + 8 + 48 <= o->n; ++i)
        if (!memcmp(o->b + i, "VRTX", 4)) {
            for (int k = 0; k < 12; ++k) { uint32_t u; memcpy(&u, &corners[k], 4); t_le32(o->b + i + 8 + 4 * k, u); }
            break;
        }
}

/* the model a cliff or ramp cell should get, worked out here from the
 * header's rule: its tileset (south-west first), letters, and the
 * variation asked; 0 on one level, 1 a cliff, 2 a ramp */
static int t_cliff_expect(const W3eGround *g, int ntypes, int i, int j, int *type, char letters[5], int *base, int *var)
{
    const W3ePoint *sw = w3e_point(g, i, j), *se = w3e_point(g, i + 1, j), *nw = w3e_point(g, i, j + 1), *ne = w3e_point(g, i + 1, j + 1);
    const W3ePoint *p[4] = { sw, se, nw, ne };
    int lo = 99, hi = -1, ramp = 0;
    for (int q = 0; q < 4; ++q) { if (p[q]->layer_height < lo) lo = p[q]->layer_height; if (p[q]->layer_height > hi) hi = p[q]->layer_height; ramp |= p[q]->is_ramp; }
    if (hi == lo) return 0;                              /* neither */
    *type = -1;
    for (int q = 0; q < 4 && *type < 0; ++q) if (p[q]->cliff_texture < ntypes) *type = p[q]->cliff_texture;
    *base = ramp ? wc3_ramp_letters(nw->layer_height, ne->layer_height, se->layer_height, sw->layer_height, nw->is_ramp, ne->is_ramp, se->is_ramp, sw->is_ramp, letters, NULL)
                 : wc3_cliff_letters(nw->layer_height, ne->layer_height, se->layer_height, sw->layer_height, letters, NULL);
    *var = sw->cliff_variation;
    return ramp ? 2 : 1;
}

static void wc3_cliff_tests(const char *dir, const char *build)
{
    char root[400], folder[520], path[900], line[1000];
    WWhy why;
    WErr e;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-wc3-cliffs", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the cliffs' fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ a cell's letters, and a sheet's columns by any capitals */
    {
        char l[5];
        int steep = 9;
        int base = wc3_cliff_letters(2, 3, 2, 2, l, &steep);
        check(base == 2 && !strcmp(l, "ABAA") && steep == 0, "a cell's letters: north-west, north-east, south-east, south-west, each its level over the lowest (ABAA: the north-east one up)");
        base = wc3_cliff_letters(4, 2, 7, 3, l, &steep);
        check(base == 2 && !strcmp(l, "CACB") && steep == 1, "a corner more than two levels up is cut to C, and said");
        static const char S[] = "ID;P\nC;X1;Y1;K\"cliffID\"\nC;X2;K\"cliffModelDir\"\nC;X1;Y2;K\"CLdi\"\nC;X2;K\"Cliffs\"\nE\n";
        Slk *k = NULL;
        e = slk_read((const unsigned char *)S, strlen(S), &k, &why);
        check(!e && slk_value_any_case(k, "CLdi", "cliffmodeldir") && !strcmp(slk_value_any_case(k, "CLdi", "CLIFFMODELDIR"), "Cliffs")
              && !slk_value(k, "CLdi", "cliffmodeldir") && slk_value(k, "CLdi", "cliffModelDir"), "a column by any capitals with slk_value_any_case; slk_value still minds them");
        slk_free(k);
    }
    /* }}} */

    /* {{{ a made map's cliffs with a made install */
    {
        /* the install: the tileset sheet (Adrt, Agrs, and Ablk, the ground
         * under CAdi's cliffs, which the map's list lacks), the cliff sheet
         * (column names in other capitals; CAdi a row, CZzz none) and three
         * cliff models (ABAA in variations 0 and 1, AAAB in 0) */
        static const char TERRAIN[] =
            "ID;PWXL;N;E\nC;X1;Y1;K\"tileID\"\nC;X2;K\"dir\"\nC;X3;K\"file\"\n"
            "C;X1;Y2;K\"Adrt\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"Adrt\"\n"
            "C;X1;Y3;K\"Agrs\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"Agrs\"\n"
            "C;X1;Y4;K\"Ablk\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"Ablk\"\nE\n";
        static const char CLIFFS[] =
            "ID;PWXL;N;E\nC;X1;Y1;K\"cliffid\"\nC;X2;K\"CLIFFMODELDIR\"\nC;X3;K\"texdir\"\nC;X4;K\"texfile\"\nC;X5;K\"groundtile\"\n"
            "C;X1;Y2;K\"CAdi\"\nC;X2;K\"Cliffs\"\nC;X3;K\"ReplaceableTextures\\Cliff\"\nC;X4;K\"Cliff0\"\nC;X5;K\"Ablk\"\nE\n";
        /* (step 1f) the editor's data naming the blight of main tileset A,
         * among other sections and rows, and its picture; one ramp model */
        static const char EDIT[] =
            "[WorldEditStrings]\r\nA=nothing,here\r\n[TileSets]\r\nL=WESTRING_L,TerrainArt\\Blight\\Lords_Blight,x\r\n"
            "  A = WESTRING_A,TerrainArt\\Blight\\Ashen_Blight,Ashenvale\r\n[Other]\r\nA=wrong,wrong\r\n";
        Bytes mdx;
        t_cliff_mdx(&mdx);
        const char *names[11] = { "TerrainArt\\Terrain.slk", "TerrainArt\\CliffTypes.slk", "TerrainArt\\Made\\Adrt.blp", "TerrainArt\\Made\\Agrs.blp", "TerrainArt\\Made\\Ablk.blp",
                                  "Doodads\\Terrain\\Cliffs\\CliffsABAA0.mdx", "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx", "Doodads\\Terrain\\Cliffs\\CliffsAAAB0.mdx",
                                  "UI\\WorldEditData.txt", "TerrainArt\\Blight\\Ashen_Blight.blp", "Doodads\\Terrain\\CliffTrans\\CliffTransAHAA0.mdx" };
        const char *bytes[11];
        size_t lens[11];
        bytes[0] = TERRAIN; lens[0] = strlen(TERRAIN);
        bytes[1] = CLIFFS; lens[1] = strlen(CLIFFS);
        for (int k = 0; k < 3; ++k) bytes[2 + k] = (const char *)t_tileset(k, 0, &lens[2 + k]);
        for (int k = 5; k < 8; ++k) { bytes[k] = (const char *)mdx.b; lens[k] = mdx.n; }
        bytes[8] = EDIT; lens[8] = strlen(EDIT);
        bytes[9] = (const char *)t_tileset(3, 0, &lens[9]);
        bytes[10] = (const char *)mdx.b; lens[10] = mdx.n;
        snprintf(folder, sizeof folder, "%s/install", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, names, bytes, lens, 11);
        for (int k = 2; k < 5; ++k) free((void *)bytes[k]);
        free((void *)bytes[9]);
        /* the map: 9 x 9 points (one chunk's 8 x 8 cells), heights 10
         * units a column east; a plateau one level up on points 3-5 x 3-5,
         * point (6, 1) two up, point (1, 6) three up (steep), point (7, 4)
         * a ramp a level up; cliff tileset CAdi, but none (15) west of
         * column 2 and CZzz (no row) on row 0; variations 1 at point
         * (2, 2), 2 at (5, 5); points (0, 0) and (1, 1) blighted */
        W3eOut o = { 0 };
        w3e_head(&o, 11, 'A', "AdrtAgrs", "CAdiCZzz", 9, 9, 0.0f, 0.0f);
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 9; ++x) {
                int layer = x >= 3 && x <= 5 && y >= 3 && y <= 5 ? 3 : 2;
                if (x == 6 && y == 1) layer = 4;
                if (x == 1 && y == 6) layer = 5;
                if (x == 7 && y == 4) layer = 3;
                int ctex = x < 2 ? 15 : y == 0 ? 1 : 0;
                int cvar = x == 2 && y == 2 ? 1 : x == 5 && y == 5 ? 2 : 0;
                int marks = (x == 7 && y == 4 ? 0x10 : 0) | ((x == 0 && y == 0) || (x == 1 && y == 1) ? 0x20 : 0);
                w3e_pt(&o, 8192 + 4 * 10 * x, 8192, (x + y) % 2, marks, 0, cvar, layer, ctex);
            }
        char map[520];
        snprintf(map, sizeof map, "%s/map", root);
        t_write(map, "war3map.w3e", o.b, o.n);
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", map);
        place.noverlays = 1;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        e = resolver_open(&place, &r, &why);
        const double s = 1 / 30.72, X0 = 1000, Y0 = 2000, step = ADT_CHUNK_SIZE / 8, x0 = X0 - 8 * step;
        Wc3Ground *w = NULL;
        if (!e) e = wc3_ground_open(r, NULL, s, x0, Y0, &w, &why);
        const Wc3Cliffs *cl = e ? NULL : wc3_ground_cliffs(w);
        check(!e && cl && wc3_ground_tilesets(w) == 4 && !strcmp(wc3_ground_tileset(w, 2)->id, "Ablk") && wc3_ground_tileset(w, 2)->source == ASSET_FROM_BLIZZARD,
              e ? why.text : "the map opens with its cliffs; the ground under CAdi's cliffs (Ablk, not in the map's list) added as a tileset of its own");
        const Wc3Tileset *bt = !e ? wc3_ground_tileset(w, wc3_ground_blight(w)) : NULL;
        check(bt && wc3_ground_blight(w) == 3 && !strcmp(bt->id, WC3_BLIGHT_ID) && !strcmp(bt->path, "TerrainArt\\Blight\\Ashen_Blight.blp") && bt->source == ASSET_FROM_BLIZZARD,
              "the blight: main tileset A's row in the editor's data's [TileSets] (its second field, .blp added), its picture from the install, the last tileset");
        if (cl) {
            const Wc3CliffType *t0 = wc3_cliffs_type(cl, 0), *t1 = wc3_cliffs_type(cl, 1);
            check(wc3_cliffs_types(cl) == 2 && t0->known && !strcmp(t0->model_dir, "Cliffs") && !strcmp(t0->picture, "ReplaceableTextures\\Cliff\\Cliff0.blp") && !strcmp(t0->ground, "Ablk")
                  && !t1->known && !strcmp(t1->model_dir, "Cliffs") && !strcmp(t1->picture, "TerrainArt\\CZzz.blp") && !t1->ground[0],
                  "each cliff tileset from the sheet, columns by any capitals; one with no row: Cliffs, the checker's picture, no ground");
            W3eGround *g = NULL;
            w3e_read(o.b, o.n, &g, &why);
            int cells = 0, ramps = 0, none = 0, falls = 0, flat_cliffs = 0, ramp_flags = 1;
            for (int j = 0; g && j < 8; ++j)
                for (int i = 0; i < 8; ++i) {
                    int type, base, var;
                    char l[5];
                    int k = t_cliff_expect(g, 2, i, j, &type, l, &base, &var);
                    ramps += k == 2;
                    if (k == 0) { flat_cliffs += wc3_cliffs_at(cl, i, j) >= 0; continue; }
                    if (k == 2) { ramp_flags &= wc3_cliffs_at(cl, i, j) >= 0 && wc3_cliffs_get(cl, wc3_cliffs_at(cl, i, j))->ramp; continue; }
                    ramp_flags &= wc3_cliffs_at(cl, i, j) < 0 || !wc3_cliffs_get(cl, wc3_cliffs_at(cl, i, j))->ramp;
                    ++cells;
                    if (type < 0) { none += wc3_cliffs_at(cl, i, j) < 0; continue; }
                    falls += !strcmp(l, "ABAA") ? var > 1 : !strcmp(l, "AAAB") ? var > 0 : 0;
                }
            check(flat_cliffs == 0 && ramp_flags, "no model on a cell on one level; a ramp cell's marked a ramp's, a cliff's not");
            Wc3CliffCounts cc;
            wc3_cliffs_counts(cl, &cc);
            snprintf(line, sizeof line, "its cliff cells counted (%d, %d of them placed, %d naming no cliff tileset, %d steep), and its ramp cells (%d, %d placed)", cc.cells, cc.placed, cc.no_tileset, cc.steep, cc.ramps, cc.ramps_placed);
            check(cc.cells == cells && cc.ramps == ramps && ramps == 4 && cc.ramps_placed == 4 && cc.ramps_no_tileset == 0 && cc.no_tileset == none && none > 0 && cc.placed == cells - none && cc.steep > 0
                  && wc3_cliffs_count(cl) == cc.placed + cc.ramps_placed, line);
            snprintf(line, sizeof line, "%d models named, %d of them in the install (the box for the rest); %d cliffs at a lower variation than asked", cc.models, cc.found, cc.fallback);
            check(cc.found == 3 && cc.models > cc.found && cc.fallback == 1 && falls == 1, line);
            /* the ramp: point (7, 4) a level up and on it; cell (6, 3) has it north-east */
            char rl[5];
            int rb = wc3_ramp_letters(2, 3, 2, 2, 0, 1, 0, 0, rl, NULL);
            int rb2 = wc3_ramp_letters(3, 2, 2, 3, 1, 1, 1, 1, rl + 0, NULL);
            check(rb == 2 && rb2 == 2 && !strcmp(rl, "HLLH"), "a ramp's letters: a ramp corner L on the lowest level and H one up (HLLH: down to the east), one off the ramp A, B, C");
            const Wc3Cliff *r63 = wc3_cliffs_get(cl, wc3_cliffs_at(cl, 6, 3));
            check(r63 && r63->ramp && !strcmp(wc3_cliffs_model(cl, r63->model), "Doodads\\Terrain\\CliffTrans\\CliffTransAHAA0.mdx" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff0.blp")
                  && !strcmp(wc3_cliffs_type(cl, 0)->ramp_dir, "CliffTrans") && near(r63->x, 7 * 128.0, 1e-9) && near(r63->y, 3 * 128.0, 1e-9),
                  "cell (6, 3) on the ramp: CliffTransAHAA0 (no rampModelDir column: CliffTrans), placed as a cliff is");
            /* cell (2, 2): corners SW (2, 2) 2, SE (3, 2) 2, NW (2, 3) 2, NE (3, 3) 3 */
            int at = wc3_cliffs_at(cl, 2, 2);
            const Wc3Cliff *c22 = wc3_cliffs_get(cl, at);
            const char *n22 = c22 ? wc3_cliffs_model(cl, c22->model) : NULL;
            check(c22 && c22->type == 0 && c22->base == 2 && n22 && !strcmp(n22, "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff0.blp"),
                  n22 ? n22 : "cell (2, 2): ABAA, variation 1 as its south-west point asks, its cliff picture after the mark");
            check(c22 && near(c22->x, 3 * 128.0, 1e-9) && near(c22->y, 2 * 128.0, 1e-9) && near(c22->z, 25, 1e-9),
                  "its origin: its south-west corner one cell east, at its corners' mean height on its lowest level (25 units)");
            const Wc3Cliff *c55 = wc3_cliffs_get(cl, wc3_cliffs_at(cl, 5, 5));
            check(c55 && !strcmp(wc3_cliffs_model(cl, c55->model), "Doodads\\Terrain\\Cliffs\\CliffsAAAB0.mdx" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff0.blp") && near(c55->z, 55, 1e-9),
                  "cell (5, 5): AAAB (its south-west corner up), variation 2 asked, none: variation 0");
            const Wc3Cliff *c50 = wc3_cliffs_get(cl, wc3_cliffs_at(cl, 5, 0)), *c15 = wc3_cliffs_get(cl, wc3_cliffs_at(cl, 1, 5));
            check(c50 && c50->type == 1 && strstr(wc3_cliffs_model(cl, c50->model), "11=TerrainArt\\CZzz.blp") && c15 && c15->type == 0 && wc3_cliffs_at(cl, 0, 5) < 0 && wc3_cliffs_at(cl, 99, 0) < 0,
                  "a cell whose south-west point names no cliff tileset takes another corner's; with none, no model; off the grid, none");
            /* its ground: the cliff cells with CAdi laid whole with Ablk */
            AdtTile *adt = calloc(1, sizeof *adt);
            adt->textures = calloc(1, sizeof *adt->textures);
            snprintf(adt->textures[0], sizeof adt->textures[0], "Tileset\\Made\\Own0.blp");
            adt->ntextures = 1;
            int tex[256][4] = { { 0 } };
            for (int k = 0; k < 256; ++k) { adt->chunks[k].x = (float)(X0 - (k / 16) * 8 * step); adt->chunks[k].y = (float)(Y0 - (k % 16) * 8 * step); }
            Wc3Lay *lay = malloc(sizeof *lay * WC3_TILE_CELLS * WC3_TILE_CELLS);
            unsigned char painted[256];
            Wc3Paint pc = { 0 };
            e = wc3_ground_lay_tile(w, adt, lay, tex, painted, &pc, &why);
            uint8_t ext[2] = { 0 };
            W3ePrep *prep = NULL;
            if (g) w3e_prep(g, ext, 2, &prep, &why);
            GroundPlace lp = { .assets = r, .wc3 = w };
            GroundPixels *lpx = malloc(sizeof *lpx * 256);
            t_lay_pixels(&lp, adt, lay, lpx);
            TLayCheck chk = { 0 };
            if (prep && !e) t_lay_check(w, prep, adt, lay, painted, lpx, NULL, &chk);
            snprintf(line, sizeof line, "the chunk laid as WC3 lays it: its %d cliff cells with a model on their cliff's ground, its ramps' on their own, its %d blighted cells with the blight on their blighted corners (%ld of %ld cells, %ld of %ld points)",
                     pc.cliff_ground, pc.blight, chk.cells_same, chk.cells, chk.same, chk.points);
            check(!e && painted[0] && pc.cliff_ground > 0 && pc.cliff_ground < cc.placed && pc.blight == 4 && chk.cells == 64 && chk.cells_same == 64 && chk.points == 4096 && chk.same == 4096, line);
            /* the south-west corner cell (WC3's (0, 0); points (0, 0) and (1, 1) blighted): the blight its highest piece, on those two corners */
            const Wc3Lay *sw = &lay[7 * WC3_TILE_CELLS];
            int bq = -1;
            for (int q = 0; q < 4 && sw->sub[q] != WC3_PIECE_NONE; ++q) if (!strcmp(adt->textures[sw->pic[q]], wc3_ground_tileset(w, wc3_ground_blight(w))->layer)) bq = q;
            check(bq > 0 && (bq == 3 || sw->sub[bq + 1] == WC3_PIECE_NONE) && sw->sub[bq] == 6, "the blight the highest piece of a blighted cell, on its blighted corners (south-west and north-east: subtile 6)");
            t_lay_pixels_free(lpx); free(lpx);
            free(lay);
            /* the cliffs into the tile's doodads */
            int placed = 0;
            e = wc3_ground_place_cliffs(w, adt, &placed, &why);
            const AdtPlace *p22 = NULL;
            int all_cliff = 1, uids = 1;
            for (int d = 0; d < adt->ndoodads; ++d) {
                const AdtPlace *p = &adt->doodads[d];
                all_cliff &= p->uid >= WC3_CLIFF_UID && p->scale == 1 && p->rot[0] == 0 && p->rot[1] == 90 && p->rot[2] == 0;
                for (int d2 = d + 1; d2 < adt->ndoodads; ++d2) uids &= p->uid != adt->doodads[d2].uid;
                if (p->uid == WC3_CLIFF_UID + 2 * 8 + 2) p22 = p;
            }
            int names_once = 1;
            for (int m = 0; m < adt->nmodels; ++m) for (int m2 = m + 1; m2 < adt->nmodels; ++m2) names_once &= strcmp(adt->models[m], adt->models[m2]) != 0;
            int named = 1;
            for (int d = 0; d < adt->ndoodads; ++d) {
                const AdtPlace *p = &adt->doodads[d];
                int k = wc3_cliffs_at(cl, (int)((p->uid - WC3_CLIFF_UID) % 8), (int)((p->uid - WC3_CLIFF_UID) / 8));
                char nm[300];
                named &= k >= 0 && wc3_cliffs_name(cl, k, nm, sizeof nm) > 0 && !strcmp(adt->models[p->name], nm);
            }
            snprintf(line, sizeof line, "every cliff and ramp placed in the tile its cell is on (%d), each its own id past the converter's, turned as WC3 lays models, each named by its model and bend, each name once (%d names for %d models)", placed, adt->nmodels, cc.models);
            check(!e && placed == cc.placed + cc.ramps_placed && adt->ndoodads == placed && all_cliff && uids && names_once && named && adt->nmodels >= cc.models && adt->nmodels <= placed, e ? why.text : line);
            /* (step 1h) its bend: the made ground rises 10 units a column east, so round cell (2, 2) (corners' mean 25)
             * each row of the 4 x 4 points is -15, -5, 5, 15 units: -240, -80, 80, 240 sixteenths */
            char want22[300];
            snprintf(want22, sizeof want22, "%s%s-240,-80,80,240,-240,-80,80,240,-240,-80,80,240,-240,-80,80,240", n22 ? n22 : "", PLACED_BEND_MARK);
            check(p22 && near(p22->x, x0 + s * 256, 1e-3) && near(p22->y, Y0 - s * 384, 1e-3) && near(p22->z, s * 25, 1e-5) && !strcmp(adt->models[p22->name], want22),
                  p22 ? adt->models[p22->name] : "cell (2, 2)'s placement: its origin by the map's rule, in yards, its model's name with its bend");
            const Wc3Cliff *c00 = NULL;
            for (int k = 0; k < wc3_cliffs_count(cl); ++k) if (wc3_cliffs_get(cl, k)->i == 7 && wc3_cliffs_get(cl, k)->j == 3) c00 = wc3_cliffs_get(cl, k);
            char nm[300];
            int edge = c00 && c00->ramp && c00->bend[3] == c00->bend[2] && c00->bend[7] == c00->bend[6] && c00->bend[2] != c00->bend[1];
            check(c00 && c00->bent && edge && wc3_cliffs_name(cl, 0, nm, 10) == -1,
                  "a ramp on the grid's east edge (cell (7, 3)): its bend's points past the edge its nearest's; a name that doesn't fit: -1");
            AdtTile *far = calloc(1, sizeof *far);
            far->chunks[0].x = (float)(X0 + 1000); far->chunks[0].y = (float)Y0;
            check(!wc3_ground_place_cliffs(w, far, &placed, &why) && placed == 0 && far->ndoodads == 0, "a tile none of the cells is on: no cliffs");
            adt_free(far); free(far);
            /* the drawer's reading of a placed name (076): the cliff picture drawn with its file */
            PlacedModel pm;
            e = placed_model_read(r, NULL, n22, s, &pm, &why);
            int replaced = 0, kept = 0;
            for (uint32_t t = 0; !e && t < pm.mdx.model.ntextures; ++t) {
                replaced += pm.mdx.model.textures[t].kind == 0 && !strcmp(pm.mdx.model.textures[t].file, "ReplaceableTextures\\Cliff\\Cliff0.blp");
                kept += !strcmp(pm.mdx.model.textures[t].file, "Textures\\Made.blp");
            }
            check(!e && pm.wc3 && replaced == 1 && kept == 1 && pm.source == ASSET_FROM_BLIZZARD && !strcmp(pm.base, "Doodads\\Terrain\\Cliffs\\CliffsABAA1"),
                  e ? why.text : "076 reads a cliff's name: the model before the mark from the install, its replaceable 11 drawn with the cliff's picture, the others as they were");
            if (!e) placed_model_free(&pm);
            check(placed_model_read(r, NULL, "Doodads\\X.mdx" PLACED_REPLACE_MARK "11", s, &pm, &why) == W_ERR_REFUSED
                  && placed_model_read(r, NULL, "Doodads\\X.mdx" PLACED_REPLACE_MARK "x=Y.blp", s, &pm, &why) == W_ERR_REFUSED, "a mark with no number and file after it: refused");
            /* (step 1h) a bend: grid point q at q units over the placement; the square's corners
             * land on grid points 5, 6, 10, 9; a point 5 units east of the origin blends 6 and 7 */
            {
                char bn[400] = "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff0.blp" PLACED_BEND_MARK;
                for (int q = 0; q < 16; ++q) { char v[16]; snprintf(v, sizeof v, q ? ",%d" : "%d", 16 * q); strncat(bn, v, sizeof bn - strlen(bn) - 1); }
                e = placed_model_read(r, NULL, bn, s, &pm, &why);
                const M2Vertex *v = e ? NULL : pm.mdx.model.vertices;
                double f = 5.0 / 128;
                int ok = v && pm.mdx.model.nvertices >= 6 && near(v[0].z, s * 5, 1e-5) && near(v[1].z, s * 6, 1e-5) && near(v[2].z, s * 10, 1e-5) && near(v[3].z, s * 9, 1e-5)
                         && near(v[0].x, -128 * s, 1e-5) && near(v[5].z, s * (5 + 6 * (1 - f) + 7 * f), 1e-5);
                for (uint32_t t = 0; !e && t < pm.mdx.model.ntextures; ++t) ok &= !(pm.mdx.model.textures[t].kind == 0 && strstr(pm.mdx.model.textures[t].file, "bend"));
                check(ok, e ? why.text : "076 bends a WC3 model by the name's grid: each point raised by the ground there, blended between the four grid points round it (the picture after the first mark kept whole)");
                if (!e) placed_model_free(&pm);
                char bn2[400] = "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx" PLACED_BEND_MARK "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff0.blp";
                e = placed_model_read(r, NULL, bn2, s, &pm, &why);
                int replaced2 = 0;
                for (uint32_t t = 0; !e && t < pm.mdx.model.ntextures; ++t) replaced2 += pm.mdx.model.textures[t].kind == 0 && !strcmp(pm.mdx.model.textures[t].file, "ReplaceableTextures\\Cliff\\Cliff0.blp");
                check(!e && replaced2 == 1 && near(pm.mdx.model.vertices[0].z, s * 6 / 16.0, 1e-5), e ? why.text : "the two marks in either order");
                if (!e) placed_model_free(&pm);
                check(placed_model_read(r, NULL, "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx" PLACED_BEND_MARK "1,2,3", s, &pm, &why) == W_ERR_REFUSED
                      && placed_model_read(r, NULL, "Doodads\\Terrain\\Cliffs\\CliffsABAA1.mdx" PLACED_BEND_MARK "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16x", s, &pm, &why) == W_ERR_REFUSED,
                      "a bend of fewer than 16 numbers, or with more after them: refused");
                e = placed_model_read(r, NULL, "Doodads\\Gone.mdx" PLACED_BEND_MARK "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16", s, &pm, &why);
                check(!e && !pm.wc3 && pm.source == ASSET_FROM_PLACEHOLDER, e ? why.text : "a bent name no place has: the box, unbent");
                if (!e) placed_model_free(&pm);
            }
            w3e_prep_free(prep);
            w3e_free(g);
            adt_free(adt); free(adt);
        }
        wc3_ground_free(w);
        if (r) resolver_free(r);
        fclose(place.log); free(log);
        free(o.b);
    }
    /* }}} */

    /* {{{ the converter's real output, when its build carries war3map.w3e */
    {
        snprintf(path, sizeof path, "%s/war3map.w3e", build);
        FILE *f = fopen(path, "rb");
        ConvertedMap m;
        if (!f || converted_map_read(build, &m, &why)) {
            if (f) fclose(f);
            printf("  (no war3map.w3e in %s: its cliff checks are skipped)\n", build);
            return;
        }
        unsigned char *wb;
        fseek(f, 0, SEEK_END);
        size_t wl = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        wb = malloc(wl);
        if (fread(wb, 1, wl, f) != wl) wl = 0;
        fclose(f);
        W3eGround *g = NULL;
        e = w3e_read(wb, wl, &g, &why);
        free(wb);
        check(!e, e ? why.text : "its war3map.w3e reads");
        if (e) return;
        printf("  the converted build folder %s: %d x %d tile points, cliff tilesets", build, g->width, g->height);
        for (int k = 0; k < g->ncliff; ++k) printf(" %.4s", g->cliff_ids + 4 * k);
        printf("\n");
        /* an install made for it: its tilesets (one picture each), the
         * cliff sheet (each cliff tileset's ground one of the map's own),
         * and, as loose files, every cliff model it needs in variation 0 */
        int nset = g->nground < 16 ? g->nground : 16, ntypes = g->ncliff < 15 ? g->ncliff : 15;
        char sheet[4000] = "ID;PWXL;N;E\nC;X1;Y1;K\"tileID\"\nC;X2;K\"dir\"\nC;X3;K\"file\"\n";
        char csheet[4000] = "ID;PWXL;N;E\nC;X1;Y1;K\"cliffID\"\nC;X2;K\"cliffModelDir\"\nC;X3;K\"texDir\"\nC;X4;K\"texFile\"\nC;X5;K\"groundTile\"\nC;X6;K\"rampModelDir\"\n";
        const char *n1[16], *b1[16], *n2[16], *b2[16];
        size_t l1[16], l2[16];
        int c1 = 0, c2 = 0;
        static char pn[16][64];
        for (int k = 0; k < nset; ++k) {
            char row[200];
            snprintf(row, sizeof row, "C;X1;Y%d;K\"%.4s\"\nC;X2;K\"TerrainArt\\Made\"\nC;X3;K\"T%02d\"\n", k + 2, g->ground_ids + 4 * k, k);
            strncat(sheet, row, sizeof sheet - strlen(sheet) - 1);
            snprintf(pn[k], sizeof pn[k], "TerrainArt\\Made\\T%02d.blp", k);
            if (k < 8) { n1[c1] = pn[k]; b1[c1] = (const char *)t_tileset(k, 0, &l1[c1]); ++c1; }
            else { n2[c2] = pn[k]; b2[c2] = (const char *)t_tileset(k, 0, &l2[c2]); ++c2; }
        }
        static const char *DIRS[2] = { "Cliffs", "CityCliffs" }, *RDIRS[2] = { "CliffTrans", "CityCliffTrans" };
        for (int k = 0; k < ntypes; ++k) {
            char row[300];
            snprintf(row, sizeof row, "C;X1;Y%d;K\"%.4s\"\nC;X2;K\"%s\"\nC;X3;K\"ReplaceableTextures\\Cliff\"\nC;X4;K\"Cliff%d\"\nC;X5;K\"%.4s\"\nC;X6;K\"%s\"\n",
                     k + 2, g->cliff_ids + 4 * k, DIRS[k % 2], k, g->ground_ids + 4 * (k % nset), RDIRS[k % 2]);
            strncat(csheet, row, sizeof csheet - strlen(csheet) - 1);
        }
        strncat(sheet, "E\n", sizeof sheet - strlen(sheet) - 1);
        strncat(csheet, "E\n", sizeof csheet - strlen(csheet) - 1);
        n1[c1] = "TerrainArt\\Terrain.slk"; b1[c1] = sheet; l1[c1] = strlen(sheet); ++c1;
        n1[c1] = "TerrainArt\\CliffTypes.slk"; b1[c1] = csheet; l1[c1] = strlen(csheet); ++c1;
        /* the editor's data naming its main tileset's blight, and the blight's picture */
        char edit[200];
        snprintf(edit, sizeof edit, "[TileSets]\r\n%c=WESTRING_MADE,TerrainArt\\Blight\\Made_Blight,Made\r\n", g->tileset_code);
        n1[c1] = "UI\\WorldEditData.txt"; b1[c1] = edit; l1[c1] = strlen(edit); ++c1;
        size_t blen;
        unsigned char *bpic = t_tileset(nset, 0, &blen);   /* its colours the blight's own: it is the last tileset */
        n1[c1] = "TerrainArt\\Blight\\Made_Blight.blp"; b1[c1] = (const char *)bpic; l1[c1] = blen; ++c1;
        snprintf(folder, sizeof folder, "%s/install-real", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, n1, b1, l1, c1);
        snprintf(path, sizeof path, "%s/War3x.mpq", folder);
        if (c2) t_write_mpq_n(path, n2, b2, l2, c2);
        for (int k = 0; k < c1 - 4; ++k) free((void *)b1[k]);
        free(bpic);
        for (int k = 0; k < c2; ++k) free((void *)b2[k]);
        /* the cells, worked out here: cliffs, ramps, models wanted, variations asked past 0 */
        int cw = g->width - 1, ch = g->height - 1, cells = 0, ramps = 0, none = 0, ramps_none = 0, asked = 0;
        char models[400][64];
        int nmodels = 0;
        char overlay[520];
        snprintf(overlay, sizeof overlay, "%s/models", root);
        Bytes mdx;
        t_cliff_mdx(&mdx);
        for (int j = 0; j < ch; ++j)
            for (int i = 0; i < cw; ++i) {
                int type, base, var;
                char l[5];
                int k = t_cliff_expect(g, ntypes, i, j, &type, l, &base, &var);
                if (!k) continue;
                if (k == 2) ++ramps; else ++cells;
                if (type < 0) { if (k == 2) ++ramps_none; else ++none; continue; }
                asked += var > 0;
                char name[64];
                const char *dirn = k == 2 ? RDIRS[type % 2] : DIRS[type % 2];
                snprintf(name, sizeof name, "%s/%s%s0.mdx", dirn, dirn, l);
                int seen = 0;
                for (int n = 0; n < nmodels && !seen; ++n) seen = !strcmp(models[n], name);
                if (!seen && nmodels < 400) {
                    snprintf(models[nmodels++], sizeof models[0], "%s", name);
                    snprintf(path, sizeof path, "Doodads/Terrain/%s", name);
                    t_write(overlay, path, mdx.b, mdx.n);
                }
            }
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", build);
        snprintf(place.overlays[1], sizeof place.overlays[1], "%s", overlay);
        place.noverlays = 2;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        Wc3Ground *w = NULL;
        e = resolver_open(&place, &r, &why);
        if (!e) e = wc3_ground_open(r, NULL, m.wc3_scale, m.wc3_x0, m.wc3_y0, &w, &why);
        const Wc3Cliffs *cl = e ? NULL : wc3_ground_cliffs(w);
        check(!e && cl, e ? why.text : "with an install made for its tilesets and cliffs, its cliffs are worked out");
        if (cl) {
            Wc3CliffCounts cc;
            wc3_cliffs_counts(cl, &cc);
            snprintf(line, sizeof line, "its %d cliff cells (%d placed, %d naming no cliff tileset) and %d ramp cells (%d placed, %d naming none), as counted here",
                     cc.cells, cc.placed, cc.no_tileset, cc.ramps, cc.ramps_placed, cc.ramps_no_tileset);
            check(cc.cells == cells && cc.ramps == ramps && cc.no_tileset == none && cc.placed == cells - none && cc.ramps_no_tileset == ramps_none && cc.ramps_placed == ramps - ramps_none && cells > 0, line);
            W3eStats st;
            w3e_stats(g, &st);
            snprintf(line, sizeof line, "its blight named by the editor's data for main tileset %c: tileset %d (%s), for its %d blighted points", g->tileset_code, wc3_ground_blight(w),
                     wc3_ground_blight(w) >= 0 ? wc3_ground_tileset(w, wc3_ground_blight(w))->path : "none", st.blight);
            check(wc3_ground_blight(w) == nset && !strcmp(wc3_ground_tileset(w, nset)->path, "TerrainArt\\Blight\\Made_Blight.blp"), line);
            snprintf(line, sizeof line, "its %d cliff and ramp models, every one found in variation 0 (%d); %d asked for a later variation take 0", cc.models, cc.found, cc.fallback);
            check(cc.models == nmodels && cc.found == nmodels && cc.fallback == asked, line);
            /* every tile, prepared as 033 does: its cliffs among its doodads */
            GroundPlace gp;
            MapWorld *mw = calloc(1, sizeof *mw);
            maps_world_init(mw, root, m.id);
            e = ground_place_init(&gp, NULL, r, m.directory, mw, 0, 0, &why);
            gp.wc3 = w;
            unsigned char *hit = calloc((size_t)cw * ch, 1);
            uint8_t ext[16] = { 0 };
            W3ePrep *prep = NULL;
            w3e_prep(g, ext, nset, &prep, &why);
            TLayCheck chk = { 0 };
            int wrong_bend = 0, n_bent = 0, foot_unread = 0, foot_corners = 0;
            double foot_bent = 0, foot_flat = 0;
            int tiles = 0, placed = 0, n_ramps = 0, twice = 0, wrong_name = 0, wrong_cell = 0, own_kept = 1;
            double below = 0, above = 0;
            char sample[260] = "";
            for (int a1 = 0; !e && a1 < 64; ++a1)
                for (int b1x = 0; !e && b1x < 64; ++b1x) {
                    AssetAnswer a;
                    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", m.directory, m.directory, a1, b1x);
                    if (asset_read(r, NULL, path, ASSET_GROUND, &a, &why)) continue;
                    AdtTile *own = malloc(sizeof *own);
                    int own_n = -1;
                    if (!adt_read(a.data, a.len, 0, own, &why)) { own_n = own->ndoodads; adt_free(own); }
                    free(own);
                    asset_answer_free(&a);
                    GroundPrep *gpr = NULL;
                    e = ground_prep_tile(&gp, b1x, a1, NULL, NULL, &gpr, &why);
                    if (e) break;
                    ++tiles;
                    const AdtTile *t = gpr->adt;
                    int n_cliffs = 0;
                    for (int d = 0; d < t->ndoodads; ++d) {
                        const AdtPlace *p = &t->doodads[d];
                        if (p->uid < WC3_CLIFF_UID) continue;
                        ++n_cliffs;
                        uint32_t c = p->uid - WC3_CLIFF_UID;
                        int i = (int)(c % (uint32_t)cw), j = (int)(c / (uint32_t)cw);
                        if (c >= (uint32_t)(cw * ch)) { ++wrong_cell; continue; }
                        if (hit[c]) ++twice;
                        hit[c] = 1;
                        /* its origin back by the rule: one cell east of its cell's south-west corner */
                        double wc3_y = (p->x - m.wc3_x0) / m.wc3_scale, wc3_x = (m.wc3_y0 - p->y) / m.wc3_scale;
                        if (fabs(wc3_x - (g->offset_x + (i + 1) * 128.0)) > 0.05 || fabs(wc3_y - (g->offset_y + j * 128.0)) > 0.05) ++wrong_cell;
                        int type, base, var;
                        char l[5], want[200];
                        int kind = t_cliff_expect(g, ntypes, i, j, &type, l, &base, &var);
                        const char *dirn = kind == 2 ? RDIRS[type % 2] : DIRS[type % 2];
                        n_ramps += kind == 2;
                        snprintf(want, sizeof want, "Doodads\\Terrain\\%s\\%s%s0.mdx" PLACED_REPLACE_MARK "11=ReplaceableTextures\\Cliff\\Cliff%d.blp", dirn, dirn, l, type);
                        const char *full = t->models[p->name], *bm = strstr(full, PLACED_BEND_MARK);
                        size_t plen = bm ? (size_t)(bm - full) : strlen(full);
                        if (plen != strlen(want) || strncmp(full, want, plen)) ++wrong_name;
                        else if (!sample[0]) snprintf(sample, sizeof sample, "%s", want);
                        /* (step 1h) its bend, worked out here: the 4 x 4 points round its cell less its corners' mean */
                        {
                            const W3ePoint *cp[4] = { w3e_point(g, i, j), w3e_point(g, i + 1, j), w3e_point(g, i, j + 1), w3e_point(g, i + 1, j + 1) };
                            double mean = (cp[0]->height + cp[1]->height + cp[2]->height + cp[3]->height) / 4;
                            char wb[200] = "";
                            int any = 0;
                            for (int q = 0; q < 16; ++q) {
                                int pi = i - 1 + q % 4, pj = j - 1 + q / 4;
                                pi = pi < 0 ? 0 : pi > cw ? cw : pi;
                                pj = pj < 0 ? 0 : pj > ch ? ch : pj;
                                int v = (int)lround((w3e_point(g, pi, pj)->height - mean) * 16);
                                any |= v != 0;
                                char x[16];
                                snprintf(x, sizeof x, q ? ",%d" : "%d", v);
                                strncat(wb, x, sizeof wb - strlen(wb) - 1);
                            }
                            if (any ? !bm || strcmp(bm + strlen(PLACED_BEND_MARK), wb) : bm != NULL) ++wrong_bend;
                            n_bent += any;
                        }
                        /* its height against the converted ground's corners of that cell */
                        double cx = g->offset_x + (i + 0.5) * 128.0, cy = g->offset_y + (j + 0.5) * 128.0;
                        double wx = m.wc3_x0 + m.wc3_scale * cy, wy = m.wc3_y0 - m.wc3_scale * cx;
                        int k = (int)floor((t->chunks[0].x - wx) / ADT_CHUNK_SIZE) * 16 + (int)floor((t->chunks[0].y - wy) / ADT_CHUNK_SIZE);
                        if (k < 0 || k > 255) { ++wrong_cell; continue; }
                        const AdtChunk *ck = &t->chunks[k];
                        const double st = ADT_CHUNK_SIZE / 8;
                        int rr = (int)floor((ck->x - wx) / st), cc2 = (int)floor((ck->y - wy) / st);
                        double lo = 1e30, hi = -1e30;
                        for (int q = 0; q < 4; ++q) {
                            double hgt = ck->z + ck->heights[adt_corner(rr + (q >> 1), cc2 + (q & 1))];
                            if (hgt < lo) lo = hgt;
                            if (hgt > hi) hi = hgt;
                        }
                        if (lo - p->z > below) below = lo - p->z;
                        if (p->z - hi > above) above = p->z - hi;
                        /* (step 1h) its foot through the drawer's reader (076, the bend in its name): at each corner
                         * on its lowest level, the made model's floor corner against the converted ground there,
                         * and how far the unbent model's (its placement's height alone) was */
                        PlacedModel fm;
                        if (placed_model_read(r, NULL, full, m.wc3_scale, &fm, &why) || !fm.wc3 || fm.mdx.model.nvertices < 4) { ++foot_unread; continue; }
                        static const int CE[4] = { 0, 1, 1, 0 }, CN[4] = { 0, 0, 1, 1 };   /* the square's corners: south-west, south-east, north-east, north-west */
                        int lowest = 99;
                        for (int q = 0; q < 4; ++q) { int lh = w3e_point(g, i + CE[q], j + CN[q])->layer_height; if (lh < lowest) lowest = lh; }
                        for (int q = 0; q < 4; ++q) {
                            if (w3e_point(g, i + CE[q], j + CN[q])->layer_height != lowest) continue;
                            double ground = ck->z + ck->heights[adt_corner(rr + 1 - CN[q], cc2 + CE[q])];
                            double bent_off = fabs(p->z + fm.mdx.model.vertices[q].z - ground), flat_off = fabs(p->z - ground);
                            if (bent_off > foot_bent) foot_bent = bent_off;
                            if (flat_off > foot_flat) foot_flat = flat_off;
                            ++foot_corners;
                        }
                        placed_model_free(&fm);
                    }
                    placed += n_cliffs;
                    if (own_n >= 0 && t->ndoodads - n_cliffs != own_n) own_kept = 0;
                    unsigned char painted[256] = { 0 };
                    for (int k = 0; gpr->wc3_lay && k < 256; ++k) painted[k] = gpr->wc3_lay[(size_t)(k / 16 * 8) * WC3_TILE_CELLS + (size_t)(k % 16 * 8)].sub[0] != WC3_PIECE_NONE;
                    if (gpr->wc3_lay) t_lay_check(w, prep, gpr->adt, gpr->wc3_lay, painted, gpr->pixels, NULL, &chk);
                    ground_prep_free(gpr, 0);
                }
            snprintf(line, sizeof line, "over its %d tiles every cliff and ramp placed once (%d of %d, %d of them ramps; %d twice), on the tile its cell is on, its origin one cell east of its cell's south-west corner (%d off)",
                     tiles, placed, cc.placed + cc.ramps_placed, n_ramps, twice, wrong_cell);
            check(!e && placed == cc.placed + cc.ramps_placed && n_ramps == cc.ramps_placed && twice == 0 && wrong_cell == 0 && own_kept, e ? why.text : line);
            snprintf(line, sizeof line, "each cliff's model by its corners' letters, its tileset's folder and its cliff picture (%d wrong; e.g. %s)", wrong_name, sample);
            check(wrong_name == 0 && sample[0], line);
            snprintf(line, sizeof line, "each cliff and ramp on uneven ground (%d of %d) named with its bend, the ground round its cell less its corners' mean; the rest without (%d wrong)", n_bent, placed, wrong_bend);
            check(wrong_bend == 0 && n_bent > 0, line);
            snprintf(line, sizeof line, "each bent cliff's and ramp's foot on the converted ground at its corners on its lowest level, read through 076: at most %.4f yards off (unbent: up to %.2f) over %d corners (%d unread)",
                     foot_bent, foot_flat, foot_corners, foot_unread);
            check(foot_unread == 0 && foot_corners > 0 && foot_bent < 0.02 && foot_flat > foot_bent, line);
            snprintf(line, sizeof line, "each cliff's foot on its own level: less than a level (%.2f yards) below its cell's lowest converted corner (at most %.2f: unbent, at its corners' mean height), never above its highest (%.2f)",
                     128 * m.wc3_scale, below, above);
            check(below < 128 * m.wc3_scale && above < 0.01, line);
            snprintf(line, sizeof line, "the ground with its cliff cells on their cliffs' ground, its ramps on their own and its blight on its blighted corners, as WC3 lays it: %ld of %ld cells, %ld of %ld blend points (%.1f%%; %ld of %ld in its %d chunks laying more than four tilesets)",
                     chk.cells_same, chk.cells, chk.same, chk.points, chk.points ? 100.0 * chk.same / chk.points : 0.0, chk.many_same, chk.many_points, chk.many_chunks);
            check(chk.points > 0 && chk.cells_same == chk.cells && chk.same == chk.points, line);
            /* one placed name through the drawer's reader: the loose model, its cliff picture */
            if (sample[0]) {
                PlacedModel pm;
                e = placed_model_read(r, NULL, sample, m.wc3_scale, &pm, &why);
                int replaced = 0;
                for (uint32_t x = 0; !e && x < pm.mdx.model.ntextures; ++x) replaced += pm.mdx.model.textures[x].kind == 0 && strstr(pm.mdx.model.textures[x].file, "ReplaceableTextures\\Cliff\\Cliff");
                check(!e && pm.wc3 && replaced == 1 && pm.source == ASSET_FROM_OVERLAY, e ? why.text : "one placed cliff through 076: a WC3 model from the models' folder, drawn with its cliff picture");
                if (!e) placed_model_free(&pm);
            }
            free(hit);
            w3e_prep_free(prep);
            maps_world_free(mw); free(mw);
        }
        wc3_ground_free(w);
        if (r) resolver_free(r);
        fclose(place.log); free(log);
        w3e_free(g);
    }
    /* }}} */
}
/* }}} */

/* {{{ a converted map's water as Warcraft III draws it (081, 1109 step 1h):
 * WC3's water sheet's row for the map's tileset, its frames, and each
 * point's colour by its depth. Files and archives made here; the
 * converter's real output too when the build folder carries war3map.w3e. */

/* the made sheet's head: its columns, the colours alpha first as WC3's are */
static void t_water_head(char *s, size_t n)
{
    static const char *G[4] = { "Smin_", "Smax_", "Dmin_", "Dmax_" }, CH[4] = { 'A', 'R', 'G', 'B' };
    snprintf(s, n, "ID;PWXL;N;E\nC;X1;Y1;K\"waterID\"\nC;X2;K\"texFile\"\nC;X3;K\"numTex\"\nC;X4;K\"texRate\"\nC;X5;K\"height\"\n");
    for (int c = 0; c < 16; ++c) {
        char cell[40];
        snprintf(cell, sizeof cell, "C;X%d;K\"%s%c\"\n", 6 + c, G[c / 4], CH[c % 4]);
        strncat(s, cell, n - strlen(s) - 1);
    }
}
/* one row: `file` and `ntex` left out when NULL, Smin_R given as `smin_r`
 * when not NULL; the colours (RGBA) shallow (10, 20, 30, 100) to (74, 84,
 * 94, 164), deep (200, 150, 100, 200) to (0, 50, 100, 255) */
static void t_water_row(char *s, size_t n, int y, const char *id, const char *file, const char *ntex, const char *smin_r)
{
    static const int C[16] = { 100, 10, 20, 30, 164, 74, 84, 94, 200, 200, 150, 100, 255, 0, 50, 100 };
    char row[1200];
    size_t at = (size_t)snprintf(row, sizeof row, "C;X1;Y%d;K\"%s\"\n", y, id);
    if (file) at += (size_t)snprintf(row + at, sizeof row - at, "C;X2;K\"%s\"\n", file);
    if (ntex) at += (size_t)snprintf(row + at, sizeof row - at, "C;X3;K%s\n", ntex);
    at += (size_t)snprintf(row + at, sizeof row - at, "C;X4;K2\nC;X5;K-0.7\n");
    for (int c = 0; c < 16; ++c) {
        if (c == 1 && smin_r) at += (size_t)snprintf(row + at, sizeof row - at, "C;X%d;K%s\n", 6 + c, smin_r);
        else at += (size_t)snprintf(row + at, sizeof row - at, "C;X%d;K%d\n", 6 + c, C[c]);
    }
    strncat(s, row, n - strlen(s) - 1);
}
/* a made frame: 4 x 4, red f x 50 */
static unsigned char *t_water_frame(int f, size_t *len)
{
    unsigned char px[64];
    for (int i = 0; i < 16; ++i) { px[i * 4] = (unsigned char)(f * 50); px[i * 4 + 1] = 0; px[i * 4 + 2] = 0; px[i * 4 + 3] = 255; }
    return t_blp2(4, 4, px, len);
}
static int t_rgba_is(const uint8_t a[4], int r, int g, int b, int al) { return a[0] == r && a[1] == g && a[2] == b && a[3] == al; }
/* WC3's colour rule worked out here: the RGBA at depth d (cells), then
 * tinted by `tint` (RGBA) when given */
static void t_water_rule(double d, const uint8_t *tint, int out[4])
{
    static const int SMIN[4] = { 10, 20, 30, 100 }, SMAX[4] = { 74, 84, 94, 164 }, DMIN[4] = { 200, 150, 100, 200 }, DMAX[4] = { 0, 50, 100, 255 };
    const int *lo = SMIN, *hi = SMAX;
    double f;
    if (d * 128 <= 64) f = d * 128 < 10 ? 0 : (d * 128 - 10) / 54;
    else { lo = DMIN; hi = DMAX; f = d * 128 >= 72 ? 1 : (d * 128 - 64) / 8; }
    for (int c = 0; c < 4; ++c) out[c] = (int)floor((lo[c] + (hi[c] - lo[c]) * f) * (tint ? tint[c] / 255.0 : 1) + 0.5);
}
static int t_rgba_near(const uint8_t a[4], const int b[4]) { for (int c = 0; c < 4; ++c) if (abs(a[c] - b[c]) > 1) return 0; return 1; }
/* a made war3map.w3i: `version` (18, 25, 28, 31), its flags, the main
 * tileset L, fog style 1 from 1000 to 4000, density 0.25, coloured `fog`,
 * the water tint `water`; `*tint_end` the byte after the tint (or, for
 * 18, after the tileset); some of what follows after it */
static void t_w3i(W3eOut *o, int version, uint32_t flags, const uint8_t fog[4], const uint8_t water[4], size_t *tint_end)
{
    o->n = 0;
    wo32(o, (uint32_t)version); wo32(o, 3); wo32(o, 6052);
    if (version >= 28) { wo32(o, 1); wo32(o, 31); wo32(o, 1); wo32(o, 12345); }
    wo_raw(o, "A Name", 7); wo_raw(o, "An Author", 10); wo_raw(o, "", 1); wo_raw(o, "1-8", 4);
    for (int k = 0; k < 8; ++k) wof(o, (float)(k * 100 - 300));
    for (int k = 0; k < 4; ++k) wo32(o, 6);
    wo32(o, 84); wo32(o, 52);
    wo32(o, flags); wo8(o, 'L');
    if (version < 25) {                                  /* Reign of Chaos's own fields, not read */
        static const char RT[] = "text\0title\0sub";
        wo32(o, 0); wo_raw(o, RT, sizeof RT); wo32(o, 2);
        *tint_end = o->n - 8 - sizeof RT;
        return;
    }
    wo32(o, (uint32_t)-1); wo_raw(o, "\0text\0title\0", 13);   /* its model, text, title, subtitle */
    wo32(o, 0);
    wo_raw(o, "\0\0\0\0", 4);
    wo32(o, 1); wof(o, 1000); wof(o, 4000); wof(o, 0.25f); wo_raw(o, fog, 4);
    wo_raw(o, "RAhr", 4); wo_raw(o, "Default", 8); wo8(o, 'L'); wo_raw(o, water, 4);
    *tint_end = o->n;
    if (version >= 28) wo32(o, 1);
    if (version >= 31) { wo32(o, 3); wo32(o, 2); }
    wo32(o, 0); wo32(o, 0);                              /* no players, no forces */
}

static void wc3_water_tests(const char *dir, const char *build)
{
    char root[400], folder[520], path[900], line[1000];
    WWhy why;
    WErr e;
    if (snprintf(root, sizeof root, "%s/tmp/shared-memory/test-wc3-water", dir) >= (int)sizeof root) { check(0, "the project folder's name is too long for the water's fixtures"); return; }
    t_remove_tree(root);
    t_mkdirs(root);

    /* {{{ the colour rule, from the sheet's colours */
    {
        Wc3WaterKind k = { .shallow_min = { 10, 20, 30, 100 }, .shallow_max = { 74, 84, 94, 164 }, .deep_min = { 200, 150, 100, 200 }, .deep_max = { 0, 50, 100, 255 } };
        uint8_t c[6][4];
        wc3_water_colour(&k, 0, c[0]);
        wc3_water_colour(&k, 10.0 / 128, c[1]);
        wc3_water_colour(&k, 37.0 / 128, c[2]);
        wc3_water_colour(&k, 64.0 / 128, c[3]);
        wc3_water_colour(&k, 66.0 / 128, c[4]);
        wc3_water_colour(&k, 68.0 / 128, c[5]);
        check(t_rgba_is(c[0], 10, 20, 30, 100) && t_rgba_is(c[1], 10, 20, 30, 100) && t_rgba_is(c[2], 42, 52, 62, 132) && t_rgba_is(c[3], 74, 84, 94, 164),
              "WC3's water colour: shallow from its least (10/128 of a cell deep and less) to its most at 64/128, halfway at 37/128");
        uint8_t d[2][4];
        wc3_water_colour(&k, 72.0 / 128, d[0]);
        wc3_water_colour(&k, 5, d[1]);
        check(t_rgba_is(c[4], 150, 125, 100, 214) && t_rgba_is(c[5], 100, 100, 100, 228) && t_rgba_is(d[0], 0, 50, 100, 255) && t_rgba_is(d[1], 0, 50, 100, 255),
              "past 64/128 deep, from its least to its most at 72/128 (a quarter at 66/128, half at 68/128), its most beyond");
    }
    /* }}} */

    /* {{{ the map's settings (083): its flags, tileset, fog and water tint */
    {
        static const uint8_t FOG[4] = { 30, 120, 255, 200 }, WATER[4] = { 50, 255, 128, 240 };
        W3eOut o = { 0 };
        size_t end;
        W3iInfo in;
        int all = 1;
        static const int V[3] = { 25, 28, 31 };
        for (int k = 0; k < 3; ++k) {
            t_w3i(&o, V[k], 0x1fc7a, FOG, WATER, &end);
            e = w3i_read(o.b, o.n, &in, &why);
            all &= !e && in.version == V[k] && in.flags == 0x1fc7a && in.tileset_code == 'L' && in.has_environment && in.fog_style == 1 && in.fog_start == 1000
                   && in.fog_end == 4000 && in.fog_density == 0.25f && t_rgba_is(in.fog_rgba, 30, 120, 255, 200) && t_rgba_is(in.water_rgba, 50, 255, 128, 240) && in.water_tinted;
        }
        check(all, "war3map.w3i, versions 25, 28 (the game's version after the editor's) and 31: its flags, tileset, fog (style, heights, density, colour red first) and water tint, used (flag 0x10000)");
        t_w3i(&o, 25, 0xfc7a, FOG, WATER, &end);
        e = w3i_read(o.b, o.n, &in, &why);
        check(!e && !in.water_tinted && t_rgba_is(in.water_rgba, 50, 255, 128, 240), "without flag 0x10000: the tint read, not used");
        t_w3i(&o, 18, 0x10000, FOG, WATER, &end);
        e = w3i_read(o.b, o.n, &in, &why);
        check(!e && in.version == 18 && in.tileset_code == 'L' && !in.has_environment && !in.water_tinted && t_rgba_is(in.water_rgba, 255, 255, 255, 255) && in.fog_style == 0,
              "version 18 (Reign of Chaos): its flags and tileset; no fog, no tint (white)");
        /* each shorter file than its fields: refused, naming where it ends */
        t_w3i(&o, 31, 0x10000, FOG, WATER, &end);
        int cut_ok = 1, named = 1;
        for (size_t n = 0; n < end; ++n) {
            e = w3i_read(o.b, n, &in, &why);
            cut_ok &= e == W_ERR_CORRUPT;
            named &= strstr(why.text, "ends inside its") != NULL;
        }
        e = w3i_read(o.b, end, &in, &why);
        check(cut_ok && named && !e && in.water_tinted, "a version 31 file cut anywhere before its water tint's end: refused as broken, naming the field; cut just after it: read");
        e = w3i_read(o.b, o.n, &in, &why);
        int bad_ok = !e;
        static const uint32_t BADV[3] = { 17, 20, 24 };
        for (int k = 0; k < 3; ++k) {
            t_le32(o.b, BADV[k]);
            bad_ok &= w3i_read(o.b, o.n, &in, &why) == W_ERR_UNSUPPORTED && strstr(why.text, "version") != NULL;
        }
        check(bad_ok, "versions 17, 20 and 24: refused as not supported, naming the version");
        free(o.b);
    }
    /* }}} */

    /* {{{ a made map's water with a made install */
    {
        /* the install: the sheet (L's row good; A's frames past the third
         * missing; B's numTex 0; C's Smin_R 300; D's texFile missing) and
         * three frames; the map's folder has its own second frame */
        char sheet[12000];
        t_water_head(sheet, sizeof sheet);
        t_water_row(sheet, sizeof sheet, 2, "LSha", "ReplaceableTextures\\Water\\Water", "3", NULL);
        t_water_row(sheet, sizeof sheet, 3, "ASha", "ReplaceableTextures\\Water\\Water", "4", NULL);
        t_water_row(sheet, sizeof sheet, 4, "BSha", "ReplaceableTextures\\Water\\Water", "0", NULL);
        t_water_row(sheet, sizeof sheet, 5, "CSha", "ReplaceableTextures\\Water\\Water", "3", "300");
        t_water_row(sheet, sizeof sheet, 6, "DSha", NULL, "3", NULL);
        strncat(sheet, "E\n", sizeof sheet - strlen(sheet) - 1);
        const char *names[4] = { "TerrainArt\\Water.slk", "ReplaceableTextures\\Water\\Water00.blp", "ReplaceableTextures\\Water\\Water01.blp", "ReplaceableTextures\\Water\\Water02.blp" };
        const char *bytes[4] = { sheet };
        size_t lens[4] = { strlen(sheet) };
        for (int f = 0; f < 3; ++f) bytes[f + 1] = (const char *)t_water_frame(f, &lens[f + 1]);
        snprintf(folder, sizeof folder, "%s/install", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, names, bytes, lens, 4);
        for (int f = 1; f < 4; ++f) free((void *)bytes[f]);
        char map[520];
        snprintf(map, sizeof map, "%s/map", root);
        size_t flen;
        unsigned char *own = t_water_frame(4, &flen);
        t_write(map, "ReplaceableTextures/Water/Water01.blp", own, flen);
        free(own);
        /* the map: 4 x 2 points, its water at 217.5 stored (127.9 drawn);
         * point 0 dry, 1 marked but its ground above the water, 2 to 7 wet
         * 7.9, 27.9, 63.9, 67.9, 227.9 and (a level down) 255.9 units deep */
        static const int H[8] = { 0, 200, 120, 100, 64, 60, -100, 0 };
        const unsigned water = 9062;
        W3eOut o = { 0 };
        W3eGround *g[2] = { NULL, NULL };
        for (int v = 0; v < 2; ++v) {
            w3e_head(&o, 11, v ? 'Z' : 'L', "Ldrt", "CLdi", 4, 2, 0.0f, 0.0f);
            for (int p = 0; p < 8; ++p) w3e_pt(&o, 8192 + 4 * H[p], water, 0, p ? 0x40 : 0, 0, 0, p == 7 ? 1 : 2, 15);
            e = w3e_read(o.b, o.n, &g[v], &why);
            check(!e, e ? why.text : "the made map's ground reads");
        }
        free(o.b);
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", map);
        place.noverlays = 1;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        if (!g[0] || !g[1]) goto made_done;
        e = resolver_open(&place, &r, &why);
        check(!e, e ? why.text : "a resolver with the map's folder and the made install opens");
        if (e) goto made_done;
        Wc3Water *w = NULL;
        const double WS = 1 / 30.72, WX0 = 1000, WY0 = 2000;    /* the map's rule: point (i, j) at (WX0 + WS 128 j, WY0 - WS 128 i) */
        e = wc3_water_open(r, NULL, g[0], WS, WX0, WY0, &w, &why);
        check(!e, e ? why.text : "the map's WC3 water opens");
        if (!e) {
            const Wc3WaterKind *k = wc3_water_kind(w);
            check(!strcmp(k->row, "LSha") && !strcmp(k->tex_file, "ReplaceableTextures\\Water\\Water") && k->frames == 3 && k->rate == 2 && fabs(k->height + 0.7) < 1e-9
                  && t_rgba_is(k->shallow_min, 10, 20, 30, 100) && t_rgba_is(k->shallow_max, 74, 84, 94, 164) && t_rgba_is(k->deep_min, 200, 150, 100, 200) && t_rgba_is(k->deep_max, 0, 50, 100, 255),
                  "its row by its tileset's letter and Sha (LSha): its frames' path, three frames, two a second, its height, the colours read alpha first into RGBA");
            const Wc3WaterFrame *f0 = wc3_water_frame(w, 0), *f1 = wc3_water_frame(w, 1), *f2 = wc3_water_frame(w, 2);
            check(f0 && f1 && f2 && !wc3_water_frame(w, 3) && !wc3_water_frame(w, -1)
                  && !strcmp(f0->path, "ReplaceableTextures\\Water\\Water00.blp") && !strcmp(f2->path, "ReplaceableTextures\\Water\\Water02.blp")
                  && f0->source == ASSET_FROM_BLIZZARD && f2->source == ASSET_FROM_BLIZZARD && f1->source == ASSET_FROM_OVERLAY
                  && f0->width == 4 && f0->height == 4 && f0->rgba[0] == 0 && f1->rgba[0] == 200 && f2->rgba[0] == 100,
                  "its frames Water00 to Water02, through the resolver: the map's own second frame before the install's");
            check(wc3_water_frame_at(w, 0) == 0 && wc3_water_frame_at(w, 0.49) == 0 && wc3_water_frame_at(w, 0.5) == 1 && wc3_water_frame_at(w, 1) == 2
                  && wc3_water_frame_at(w, 1.5) == 0 && wc3_water_frame_at(w, -1) == 0 && wc3_water_frame_at(w, 100.2) == 2,
                  "the frame shown: two a second, round the three, 0 before the start");
            Wc3WaterCounts n = wc3_water_counts(w);
            static const double D[8] = { 0, 0, 7.9, 27.9, 63.9, 67.9, 227.9, 255.9 };
            int points_ok = 1;
            for (int p = 0; p < 8; ++p) {
                double d = -1;
                uint8_t c[4];
                int wet = wc3_water_point(w, p % 4, p / 4, &d, c), ex[4];
                if (p < 2) { points_ok &= !wet; continue; }
                t_water_rule(D[p] / 128, NULL, ex);
                points_ok &= wet && fabs(d - D[p] / 128) < 1e-6 && t_rgba_is(c, ex[0], ex[1], ex[2], ex[3]);
            }
            points_ok &= !wc3_water_point(w, 4, 0, NULL, NULL) && !wc3_water_point(w, 0, 2, NULL, NULL) && !wc3_water_point(w, -1, 0, NULL, NULL);
            check(points_ok, "each point: dry, or its ground above the water, not wet; each wet one its depth (the drawn surface less its ground, a level down counted) and its colour by the rule; outside the grid none");
            snprintf(line, sizeof line, "the map's points: %d wet, %d shallow, %d deep, the deepest %.4f cells", n.wet, n.shallow, n.deep, n.deepest);
            check(n.wet == 6 && n.shallow == 3 && n.deep == 3 && fabs(n.deepest - 255.9 / 128) < 1e-6, line);
            check(!wc3_water_info(w) && !k->tinted && t_rgba_is(k->tint, 255, 255, 255, 255), "no war3map.w3i in any place: untinted");
            /* a world place to its point, by the map's rule: every point's
             * depth (units), a dry one's too, its colour by it */
            static const double DA[8] = { 127.9, -72.1, 7.9, 27.9, 63.9, 67.9, 227.9, 255.9 };
            int at_ok = 1, ex[4];
            for (int p = 0; p < 8; ++p) {
                double z = 0;
                uint8_t c[4];
                int i = p % 4, j = p / 4;
                t_water_rule(DA[p] / 128, NULL, ex);
                at_ok &= wc3_water_at(w, WX0 + WS * 128 * j, WY0 - WS * 128 * i, &z, c) == 1 && fabs(z - 127.9 * WS) < 1e-9 && t_rgba_is(c, ex[0], ex[1], ex[2], ex[3]);
            }
            uint8_t c[4];
            double z;
            t_water_rule(DA[0] / 128, NULL, ex);
            int between = !wc3_water_at(w, WX0 + WS * 64, WY0, NULL, NULL) && !wc3_water_at(w, WX0 + 0.02, WY0, NULL, NULL) && wc3_water_at(w, WX0 + 0.005, WY0, NULL, NULL);
            int off = !wc3_water_at(w, WX0 - WS * 128, WY0 + WS * 128, &z, c) && t_rgba_is(c, ex[0], ex[1], ex[2], ex[3]);
            t_water_rule(DA[7] / 128, NULL, ex);
            off &= !wc3_water_at(w, WX0 + WS * 128 * 5, WY0 - WS * 128 * 9, NULL, c) && t_rgba_is(c, ex[0], ex[1], ex[2], ex[3]);
            check(at_ok && between && off, "a world place to its point by the map's rule: on a point (within a hundredth of a yard) its surface and its colour by its depth, a dry point's or one under the ground too; between points or off the grid: not on one, the grid's nearest point's colour");

            /* {{{ a layer's points (082): one chunk over the map's south-west
             * corner, its corners (r, c) on points (c, 1 - r); cells (0, 0) to
             * (0, 2) on the grid, cell (1, 0) reaching a row south of it */
            {
                AdtTile *t = calloc(1, sizeof *t);
                const float step = ADT_CHUNK_SIZE / 8;
                t->nliquids = 1;
                t->liquids = calloc(1, sizeof *t->liquids);
                AdtLiquid *q = &t->liquids[0];
                q->chunk = 5; q->kind = 1;
                t->chunks[5].x = (float)(WX0 + step); t->chunks[5].y = (float)WY0;
                q->wet[0][0] = q->wet[0][1] = q->wet[0][2] = q->wet[1][0] = 1;
                for (int rr = 0; rr < 9; ++rr) for (int cc = 0; cc < 9; ++cc) { q->height[rr][cc] = (float)(127.9 * WS); q->depth[rr][cc] = (uint8_t)(rr * 9 + cc); }
                q->height[0][0] += 1;                    /* the converter's level over the bank at corner (0, 0) */
                const double OX = 900, OY = 1900;
                WaterVerts pv, pw;
                e = water_prep_layer(t, 0, OX, OY, NULL, &pv, &why);
                int plain = !e && pv.count == 24 && !pv.colors && !pv.on_grid && !pv.off_grid && !pv.moved
                            && fabs(pv.vertices[0] - (t->chunks[5].x - OX)) < 1e-4 && fabs(pv.vertices[1] - (t->chunks[5].y - OY)) < 1e-4 && pv.vertices[2] == q->height[0][0]
                            && fabs(pv.texcoords[0] - t->chunks[5].y / step) < 1e-4 && fabs(pv.texcoords[1] - t->chunks[5].x / step) < 1e-4 && pv.texcoords2[0] == 0;
                /* the second point of the first cell: corner (0, 1) */
                plain &= !e && fabs(pv.vertices[3] - (t->chunks[5].x - OX)) < 1e-4 && fabs(pv.vertices[4] - (t->chunks[5].y - step - OY)) < 1e-4 && fabs(pv.texcoords2[2] - 1 / 255.0f) < 1e-7;
                check(plain, "a layer's points: two triangles a wet cell (24 for 4), placed from the drawing origin, the picture once a cell by world place, the file's height and depth; no colours");
                e = water_prep_layer(t, 0, OX, OY, w, &pw, &why);
                int pts_ok = !e && pw.count == 24 && pw.colors && pw.on_grid == 21 && pw.off_grid == 3 && pw.moved == 2;
                for (int v = 0; !e && v < pw.count; ++v) {
                    int rr = (int)lround((t->chunks[5].x - (pw.vertices[v * 3] + OX)) / step), cc = (int)lround((t->chunks[5].y - (pw.vertices[v * 3 + 1] + OY)) / step);
                    int pj = 1 - rr, on = pj >= 0;
                    t_water_rule(DA[(on ? pj : 0) * 4 + cc] / 128, NULL, ex);
                    double want = on ? 127.9 * WS : q->height[rr][cc];
                    pts_ok &= fabs(pw.vertices[v * 3 + 2] - want) < 1e-5 && t_rgba_is(pw.colors + v * 4, ex[0], ex[1], ex[2], ex[3])
                              && pw.vertices[v * 3] == pv.vertices[v * 3] && pw.vertices[v * 3 + 1] == pv.vertices[v * 3 + 1] && pw.texcoords2[v * 2] == pv.texcoords2[v * 2];
                }
                check(pts_ok, "as WC3's water: each point on a tile point at that point's own surface (the bank's corner sunk to it: 2 points moved) and coloured by its depth; the 3 off the grid at the file's height, the nearest point's colour; placed as before");
                water_verts_free(&pv); water_verts_free(&pw);
                e = water_prep_layer(t, 1, OX, OY, NULL, &pv, &why);
                int refused = e == W_ERR_REFUSED;
                memset(q->wet, 0, sizeof q->wet);
                e = water_prep_layer(t, 0, OX, OY, w, &pv, &why);
                check(refused && !e && pv.count == 0 && !pv.vertices, "a layer past the tile's: refused; a layer with no wet cell: no points");
                check(strstr(WATER_VS, "in vec4 vertexColor;") && strstr(WATER_VS, "col = vertexColor;") && strstr(WATER_FS, "uniform float raw, wc3;")
                      && strstr(WATER_FS, "if (wc3 > 0.5) { vec4 w = texture(texture0, uv) * col; frag = vec4(mix(w.rgb, fog_col, fog), w.a); return; }"),
                      "the water shader's text (082): each point's colour carried to the picture, WC3's water the picture times it, alpha too, fogged and unlit");
                water_verts_free(&pv);
                free(t->liquids); free(t);
            }
            /* }}} */
            wc3_water_free(w);
        }
        /* the refusals: the converter's water stays */
        e = wc3_water_open(NULL, NULL, g[0], WS, WX0, WY0, &w, &why);
        check(e == W_ERR_NOT_FOUND && !w, "no resolver: not found");
        e = wc3_water_open(r, NULL, g[0], 0, WX0, WY0, &w, &why);
        check(e == W_ERR_REFUSED && !w, "a rule's scale of 0: refused");
        e = wc3_water_open(r, NULL, g[1], WS, WX0, WY0, &w, &why);
        check(e == W_ERR_NOT_FOUND && !w && strstr(why.text, "no row ZSha"), e ? why.text : "a tileset with no row: opened");
        static const struct { char code; WErr want; const char *says; } BAD[4] = {
            { 'A', W_ERR_NOT_FOUND, "Water03.blp is in no place" }, { 'B', W_ERR_CORRUPT, "numTex" }, { 'C', W_ERR_CORRUPT, "Smin_R is 300" }, { 'D', W_ERR_CORRUPT, "texFile is empty" } };
        int bad_ok = 1;
        for (int b = 0; b < 4; ++b) {
            g[0]->tileset_code = BAD[b].code;
            e = wc3_water_open(r, NULL, g[0], WS, WX0, WY0, &w, &why);
            bad_ok &= e == BAD[b].want && !w && strstr(why.text, BAD[b].says) != NULL;
            if (!e) wc3_water_free(w);
        }
        check(bad_ok, "a frame no place has: not found, naming it; numTex 0, a colour past 255, no texFile: refused as broken, naming the column");
        g[0]->tileset_code = 'L';
        resolver_free(r);
        r = NULL;
        /* the map's war3map.w3i in its folder: tinted by it when its flag
         * says, read and untinted when not, refused when broken */
        {
            static const uint8_t FOG[4] = { 0, 0, 0, 255 }, TINT[4] = { 128, 255, 0, 200 };
            static const struct { uint32_t flags; int version; WErr want; int tinted; } W3I[3] = { { 0x10000, 25, W_OK, 1 }, { 0x8000, 31, W_OK, 0 }, { 0x10000, 20, W_ERR_UNSUPPORTED, 0 } };
            int tint_ok = 1;
            for (int v = 0; v < 3; ++v) {
                W3eOut wo = { 0 };
                size_t end;
                t_w3i(&wo, 25, W3I[v].flags, FOG, TINT, &end);
                t_le32(wo.b, (uint32_t)W3I[v].version);
                t_write(map, "war3map.w3i", wo.b, wo.n);
                free(wo.b);
                Resolver *r2 = NULL;
                Wc3Water *w2 = NULL;
                e = resolver_open(&place, &r2, &why);
                if (!e) e = wc3_water_open(r2, NULL, g[0], WS, WX0, WY0, &w2, &why);
                tint_ok &= e == W3I[v].want;
                if (W3I[v].want) tint_ok &= !w2 && strstr(why.text, "war3map.w3i") != NULL;
                if (!e) {
                    const Wc3WaterKind *k = wc3_water_kind(w2);
                    const W3iInfo *in = wc3_water_info(w2);
                    tint_ok &= in && in->version == W3I[v].version && k->tinted == W3I[v].tinted && (!W3I[v].tinted || t_rgba_is(k->tint, 128, 255, 0, 200));
                    int ex[4];
                    uint8_t c[4];
                    for (int p = 2; p < 8; ++p) {
                        double d;
                        wc3_water_point(w2, p % 4, p / 4, &d, c);
                        t_water_rule(d, W3I[v].tinted ? TINT : NULL, ex);
                        tint_ok &= t_rgba_near(c, ex);
                    }
                    wc3_water_at(w2, WX0, WY0, NULL, c);
                    t_water_rule(127.9 / 128, W3I[v].tinted ? TINT : NULL, ex);
                    tint_ok &= t_rgba_near(c, ex);
                    wc3_water_free(w2);
                }
                if (r2) resolver_free(r2);
            }
            check(tint_ok, "the map's war3map.w3i from its folder: with its tint's flag each colour times the tint (red, green, blue and alpha over 255); without, untinted; a version it can't read: refused, naming the file");
            snprintf(path, sizeof path, "%s/war3map.w3i", map);
            remove(path);
        }
        /* no install: not found */
        place.wc3[0] = '\0';
        e = resolver_open(&place, &r, &why);
        if (!e) {
            e = wc3_water_open(r, NULL, g[0], WS, WX0, WY0, &w, &why);
            check(e == W_ERR_NOT_FOUND && strstr(why.text, "Water.slk"), "no Warcraft III install: not found (no Water.slk), so the converter's water stays");
            resolver_free(r);
            r = NULL;
        }
    made_done:
        w3e_free(g[0]); w3e_free(g[1]);
        fclose(place.log); free(log);
    }
    /* }}} */

    /* {{{ the converter's real output, when its build carries war3map.w3e */
    {
        snprintf(path, sizeof path, "%s/war3map.w3e", build);
        FILE *f = fopen(path, "rb");
        if (!f) { printf("  (no war3map.w3e in %s: its water checks are skipped)\n", build); return; }
        fseek(f, 0, SEEK_END);
        size_t wl = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *wb = malloc(wl);
        if (fread(wb, 1, wl, f) != wl) wl = 0;
        fclose(f);
        W3eGround *g = NULL;
        e = w3e_read(wb, wl, &g, &why);
        free(wb);
        check(!e, e ? why.text : "its war3map.w3e reads");
        if (e) return;
        ConvertedMap m;
        e = converted_map_read(build, &m, &why);
        check(!e, e ? why.text : "its map.lua reads");
        if (e) { w3e_free(g); return; }
        /* an install made for it: its tileset's row, three frames */
        char sheet[4000], id[8];
        t_water_head(sheet, sizeof sheet);
        snprintf(id, sizeof id, "%cSha", g->tileset_code);
        t_water_row(sheet, sizeof sheet, 2, id, "ReplaceableTextures\\Water\\Water", "3", NULL);
        strncat(sheet, "E\n", sizeof sheet - strlen(sheet) - 1);
        const char *names[4] = { "TerrainArt\\Water.slk", "ReplaceableTextures\\Water\\Water00.blp", "ReplaceableTextures\\Water\\Water01.blp", "ReplaceableTextures\\Water\\Water02.blp" };
        const char *bytes[4] = { sheet };
        size_t lens[4] = { strlen(sheet) };
        for (int k = 0; k < 3; ++k) bytes[k + 1] = (const char *)t_water_frame(k, &lens[k + 1]);
        snprintf(folder, sizeof folder, "%s/install-real", root);
        t_mkdirs(folder);
        snprintf(path, sizeof path, "%s/war3.mpq", folder);
        t_write_mpq_n(path, names, bytes, lens, 4);
        for (int k = 1; k < 4; ++k) free((void *)bytes[k]);
        ResolverPlace place = { .distinctness = 0.5 };
        snprintf(place.overlays[0], sizeof place.overlays[0], "%s", build);
        place.noverlays = 1;
        snprintf(place.wc3, sizeof place.wc3, "%s", folder);
        char *log = NULL;
        size_t log_len = 0;
        place.log = open_memstream(&log, &log_len);
        Resolver *r = NULL;
        Wc3Water *w = NULL;
        e = resolver_open(&place, &r, &why);
        if (!e) e = wc3_water_open(r, NULL, g, m.wc3_scale, m.wc3_x0, m.wc3_y0, &w, &why);
        check(!e, e ? why.text : "the converted map's WC3 water opens, its row by its tileset");
        if (!e) {
            /* its settings, when the build carries war3map.w3i (the converter's draft PR #247) */
            const W3iInfo *in = wc3_water_info(w);
            const Wc3WaterKind *kd = wc3_water_kind(w);
            if (in) {
                snprintf(line, sizeof line, "its war3map.w3i (version %d, tileset %c, flags %x): fog style %d from %.0f to %.0f, density %.2f, colour %d %d %d %d; water tint %d %d %d %d, %s",
                         in->version, in->tileset_code, in->flags, in->fog_style, in->fog_start, in->fog_end, in->fog_density, in->fog_rgba[0], in->fog_rgba[1], in->fog_rgba[2], in->fog_rgba[3],
                         in->water_rgba[0], in->water_rgba[1], in->water_rgba[2], in->water_rgba[3], kd->tinted ? "used" : "not used (no flag 0x10000)");
                check(in->has_environment && in->tileset_code == g->tileset_code && kd->tinted == ((in->flags & 0x10000) != 0) && (!kd->tinted || !memcmp(kd->tint, in->water_rgba, 4)), line);
            } else printf("  (no war3map.w3i in %s: its water is untinted)\n", build);
            /* each point worked out here from its stored numbers */
            int wet = 0, shallow = 0, deep = 0, wrong = 0;
            double deepest = 0;
            for (int j = 0; j < g->height; ++j)
                for (int i = 0; i < g->width; ++i) {
                    const W3ePoint *p = &g->points[(size_t)j * g->width + i];
                    double wz = p->water_level - 89.6, gz = p->height + (p->layer_height - 2) * 128.0, dd = (wz - gz) / 128;
                    int is = p->has_water && wz > gz, ex[4];
                    double d = -1;
                    uint8_t c[4];
                    int got = wc3_water_point(w, i, j, &d, c);
                    if (got != is) { ++wrong; continue; }
                    if (!is) continue;
                    ++wet;
                    if (dd * 128 <= 64) ++shallow; else ++deep;
                    if (dd > deepest) deepest = dd;
                    t_water_rule(dd, kd->tinted ? kd->tint : NULL, ex);
                    wrong += fabs(d - dd) > 1e-9 || abs(c[0] - ex[0]) > 1 || abs(c[1] - ex[1]) > 1 || abs(c[2] - ex[2]) > 1 || abs(c[3] - ex[3]) > 1;
                }
            Wc3WaterCounts n = wc3_water_counts(w);
            snprintf(line, sizeof line, "its %d wet points (%d shallow, %d deep, the deepest %.2f cells) each its depth and colour by WC3's rule, worked out here from the stored numbers (%d wrong)",
                     wet, shallow, deep, deepest, wrong);
            check(wet > 0 && !wrong && n.wet == wet && n.shallow == shallow && n.deep == deep && fabs(n.deepest - deepest) < 1e-9, line);

            /* the converter's water against WC3's (the plan's step 4), and
             * its points as 040 draws them (082): each ground file's layers */
            int cw = g->width - 1, ch = g->height - 1;
            unsigned char *conv = calloc((size_t)cw * ch, 1);
            long cells = 0, outside = 0, twice = 0, verts = 0, on = 0, off = 0, moved = 0, moved_wet = 0, pts_wrong = 0, layers = 0, prep_wrong = 0;
            double wet_worst = 0;
            const double s = m.wc3_scale;
            const float step = ADT_CHUNK_SIZE / 8;
            AdtTile *t = malloc(sizeof *t);
            for (int tx = 0; tx < 64; ++tx)
                for (int ty = 0; ty < 64; ++ty) {
                    AssetAnswer a;
                    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", m.directory, m.directory, tx, ty);
                    if (asset_read(r, NULL, path, ASSET_GROUND, &a, &why)) continue;
                    WErr bad = adt_read(a.data, a.len, 0, t, &why);
                    asset_answer_free(&a);
                    if (bad) { check(0, why.text); continue; }
                    for (int k = 0; k < t->nliquids; ++k) {
                        const AdtLiquid *q = &t->liquids[k];
                        const AdtChunk *c = &t->chunks[q->chunk];
                        ++layers;
                        for (int rr = 0; rr < 8; ++rr)
                            for (int cc = 0; cc < 8; ++cc) {
                                if (!q->wet[rr][cc]) continue;
                                ++cells;
                                double X = (m.wc3_y0 - (c->y - (cc + 0.5) * step)) / s, Y = (c->x - (rr + 0.5) * step - m.wc3_x0) / s;
                                int i = (int)floor((X - g->offset_x) / 128), j = (int)floor((Y - g->offset_y) / 128);
                                if (i < 0 || j < 0 || i >= cw || j >= ch) { ++outside; continue; }
                                twice += conv[j * cw + i];
                                conv[j * cw + i] = 1;
                            }
                        WaterVerts pv;
                        if (water_prep_layer(t, k, 0, 0, w, &pv, &why)) { ++prep_wrong; continue; }
                        verts += pv.count; on += pv.on_grid; off += pv.off_grid; moved += pv.moved;
                        for (int v = 0; v < pv.count; ++v) {
                            /* the point, worked out here from the place */
                            double wx = pv.vertices[v * 3], wy = pv.vertices[v * 3 + 1];
                            double fi = ((m.wc3_y0 - wy) / s - g->offset_x) / 128, fj = ((wx - m.wc3_x0) / s - g->offset_y) / 128;
                            int i = (int)lround(fi), j = (int)lround(fj), rr = (int)lround((c->x - wx) / step), cc = (int)lround((c->y - wy) / step);
                            int is_on = i >= 0 && j >= 0 && i < g->width && j < g->height;
                            int ci = i < 0 ? 0 : i >= g->width ? g->width - 1 : i, cj = j < 0 ? 0 : j >= g->height ? g->height - 1 : j;
                            const W3ePoint *p = w3e_point(g, ci, cj);
                            double wz = p->water_level - 89.6, gz = p->height + (p->layer_height - 2) * 128.0;
                            int ex[4];
                            t_water_rule((wz - gz) / 128, kd->tinted ? kd->tint : NULL, ex);
                            double want = is_on ? wz * s : q->height[rr][cc];
                            pts_wrong += fabs(pv.vertices[v * 3 + 2] - want) > 1e-3 || !t_rgba_near(pv.colors + v * 4, ex);
                            if (is_on && p->has_water && wz > gz) { double dz = fabs(q->height[rr][cc] - wz * s); if (dz > wet_worst) wet_worst = dz; moved_wet += dz > 0.001; }
                        }
                        water_verts_free(&pv);
                    }
                    adt_free(t);
                }
            free(t);
            long both = 0, conv_only = 0, wc3_only = 0;
            for (int j = 0; j < ch; ++j)
                for (int i = 0; i < cw; ++i) {
                    int any = 0;
                    for (int q = 0; q < 4; ++q) {
                        const W3ePoint *p = w3e_point(g, i + (q & 1), j + (q >> 1));
                        any |= p->has_water && p->water_level - 89.6 > p->height + (p->layer_height - 2) * 128.0;
                    }
                    both += any && conv[j * cw + i]; conv_only += !any && conv[j * cw + i]; wc3_only += any && !conv[j * cw + i];
                }
            free(conv);
            snprintf(line, sizeof line, "the converter's %ld wet cells on the grid (%ld past it, none twice) are WC3's cells with a wet corner: %ld both, %ld its alone, %ld WC3's alone; at wet corners its surface WC3's (worst %.4f yards)",
                     cells - outside, outside, both, conv_only, wc3_only, wet_worst);
            check(cells > 0 && !twice && !conv_only && !wc3_only && wet_worst < 0.001, line);
            snprintf(line, sizeof line, "its %ld water layers' %ld points drawn as WC3's (082): %ld on a tile point, at WC3's own surface and colour (%ld moved off the converter's height, at dry corners alone), %ld past the grid at the file's height (%ld wrong)",
                     layers, verts, on, moved, off, pts_wrong);
            check(verts > 0 && on + off == verts && !prep_wrong && !pts_wrong && !moved_wet, line);
            wc3_water_free(w);
        }
        if (r) resolver_free(r);
        fclose(place.log); free(log);
        w3e_free(g);
    }
    /* }}} */
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

    /* {{{ the asset resolver (105a): folders and an archive made here, no game data,
     * so before the archives, which stop the program when there are none */
    printf("the asset resolver\n");
    resolver_tests(dir);
    /* }}} */

    /* {{{ the test map's launch and receipt (1109): folders made here, no game data */
    printf("the test map's launch and its goodbye receipt\n");
    test_map_tests(dir);
    /* }}} */

    /* {{{ a converted map's folder (1109): map.lua, its tables per row, its
     * ground's heights; folders made here, and the converter's own output
     * when there is some. No game data, so before the archives */
    printf("a converted map's folder\n");
    {
        char build[700];
        if (argc > 3 && strcmp(argv[3], "-")) snprintf(build, sizeof build, "%s", argv[3]);
        else snprintf(build, sizeof build, "%s/assets/test-map-build", dir);
        converted_tests(dir, build);
    }
    /* }}} */

    /* {{{ placed models through the resolver (1109 step 1c): folders and an
     * archive made here, no game data, so before the archives */
    printf("placed models and buildings, and the ground's pictures, through the resolver\n");
    placed_tests(dir);
    /* }}} */

    /* {{{ Warcraft III's own install as a place, and a converted map's
     * ground painted with its tilesets (1109 step 1d): folders and
     * archives made here, and the converter's output when it carries the
     * map's war3map.w3e; no game data, so before the archives */
    printf("Warcraft III's own install, and a converted map's ground with WC3's tilesets\n");
    {
        char build[700];
        if (argc > 3 && strcmp(argv[3], "-")) snprintf(build, sizeof build, "%s", argv[3]);
        else snprintf(build, sizeof build, "%s/assets/test-map-build", dir);
        wc3_place_tests(dir, build);
    }
    /* }}} */

    /* {{{ a converted map's cliffs as WC3's own cliff models (1109 step 1e):
     * folders and archives made here, and the converter's output when it
     * carries war3map.w3e; no game data, so before the archives */
    printf("a converted map's cliffs as Warcraft III's own cliff models\n");
    {
        char build[700];
        if (argc > 3 && strcmp(argv[3], "-")) snprintf(build, sizeof build, "%s", argv[3]);
        else snprintf(build, sizeof build, "%s/assets/test-map-build", dir);
        wc3_cliff_tests(dir, build);
    }
    /* }}} */

    /* {{{ a converted map's water as WC3 draws it (1109 step 1h): folders
     * and archives made here, and the converter's output when it carries
     * war3map.w3e; no game data, so before the archives */
    printf("a converted map's water as Warcraft III draws it\n");
    {
        char build[700];
        if (argc > 3 && strcmp(argv[3], "-")) snprintf(build, sizeof build, "%s", argv[3]);
        else snprintf(build, sizeof build, "%s/assets/test-map-build", dir);
        wc3_water_tests(dir, build);
    }
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

    /* {{{ WC3's ground (302a): files written here, so it runs with no game data */
    check_wc3_ground();
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
        const char *war3 = argc > 4 ? argv[4] : "/mnt/mtwo/programming/ai-stuff/world-edit-to-execute/wc3-installs/frozen-throne/war3.mpq";
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
            Prefetch *reader = prefetch_start(&rc, NULL, "Kalimdor", 1);
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
            if (ground_place_init(&gp, &rc, NULL, "Kalimdor", &gw, 2700, -400, &why)) check(0, why.text);
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
