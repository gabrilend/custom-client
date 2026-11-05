/*
 * 085-baseline-set.c (issues 105a, 1109) - the smallest baseline set; see
 * 085-baseline-set.h
 *
 * For a general audience: reads the converter's two tables, makes one
 * picture for each ground the converter can paint, one stand-in model for
 * every doodad it can place and a run of water pictures, writes them into
 * the folder at their names, and lists every file in a manifest the
 * resolver reads as it reads a replacement pack's.
 */
#include "085-baseline-set.h"
#include "059-loose-folders.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TABLE_MAX ((size_t)4 << 20)   /* bytes a converter table may hold: far past either */

/* {{{ little-endian writers */
static void put16(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static void putf(unsigned char *p, float f) { uint32_t v; memcpy(&v, &f, 4); put32(p, v); }
/* }}} */

/* {{{ static blp: BLP2, plain BGRA, one level (header 148 bytes, palette
 * 1024 unused, pixels from 1172), from RGBA rows top first */
static unsigned char *blp(int side, const unsigned char *rgba, size_t *len)
{
    size_t at = 148 + 1024, n = (size_t)side * side;
    *len = at + n * 4;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "BLP2", 4);
    put32(s + 4, 1);                          /* plain, not JPEG */
    s[8] = 3; s[9] = 8; s[10] = 0; s[11] = 0; /* plain, 8 alpha bits, no smaller copies */
    put32(s + 12, (uint32_t)side); put32(s + 16, (uint32_t)side);
    put32(s + 20, (uint32_t)at); put32(s + 84, (uint32_t)(n * 4));
    for (size_t i = 0; i < n; ++i) {
        unsigned char *p = s + at + i * 4;
        p[0] = rgba[i * 4 + 2]; p[1] = rgba[i * 4 + 1]; p[2] = rgba[i * 4]; p[3] = rgba[i * 4 + 3];
    }
    return s;
}
/* }}} */

/* {{{ the pictures
 * Mottling: a lattice of 8 x 8 values from -1 to 1 (from the seed),
 * joined smoothly and wrapping, so the picture repeats with no seam, and
 * a little grain a pixel. */
static uint32_t next(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static unsigned char clamp255(double v) { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : lround(v)); }
static void mottle(const uint8_t rgb[3], uint32_t seed, double depth, unsigned char *rgba)
{
    enum { L = 8, S = BASELINE_PICTURE_SIDE };
    double lat[L][L];
    uint32_t st = seed ? seed : 0x9e3779b9u;
    for (int i = 0; i < 4; ++i) next(&st);
    for (int y = 0; y < L; ++y) for (int x = 0; x < L; ++x) lat[y][x] = (next(&st) % 2001) / 1000.0 - 1;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            double fx = x * (double)L / S, fy = y * (double)L / S;
            int x0 = (int)fx, y0 = (int)fy, x1 = (x0 + 1) % L, y1 = (y0 + 1) % L;
            double tx = fx - x0, ty = fy - y0;
            tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
            double n = (lat[y0][x0] * (1 - tx) + lat[y0][x1] * tx) * (1 - ty) + (lat[y1][x0] * (1 - tx) + lat[y1][x1] * tx) * ty;
            double grain = (next(&st) % 7) - 3.0;
            unsigned char *p = rgba + ((size_t)y * S + x) * 4;
            for (int c = 0; c < 3; ++c) p[c] = clamp255(rgb[c] * (1 + depth * n) + grain);
            p[3] = 255;
        }
}
unsigned char *baseline_ground_picture(const uint8_t rgb[3], uint32_t seed, size_t *len)
{
    unsigned char *rgba = malloc((size_t)BASELINE_PICTURE_SIDE * BASELINE_PICTURE_SIDE * 4);
    mottle(rgb, seed, 0.12, rgba);
    unsigned char *s = blp(BASELINE_PICTURE_SIDE, rgba, len);
    free(rgba);
    return s;
}
unsigned char *baseline_water_frame(int frame, size_t *len)
{
    enum { S = BASELINE_PICTURE_SIDE };
    static const double BASE[3] = { 34, 80, 120 };
    const double PI = 3.14159265358979323846, t = (double)(frame - 1) / BASELINE_WATER_FRAMES;
    unsigned char *rgba = malloc((size_t)S * S * 4);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            double r = sin(2 * PI * (2.0 * x / S + t)) + sin(2 * PI * (3.0 * y / S - t)) + 0.5 * sin(2 * PI * ((double)(x + y) / S + 2 * t));
            unsigned char *p = rgba + ((size_t)y * S + x) * 4;
            for (int c = 0; c < 3; ++c) p[c] = clamp255(BASE[c] * (1 + 0.08 * r));
            p[3] = 255;
        }
    unsigned char *s = blp(S, rgba, len);
    free(rgba);
    return s;
}
unsigned char *baseline_stand_in_picture(size_t *len)
{
    enum { S = BASELINE_PICTURE_SIDE };
    static const uint8_t STONE[3] = { 128, 124, 118 }, BAND[3] = { 60, 90, 140 };
    unsigned char *rgba = malloc((size_t)S * S * 4);
    mottle(STONE, 0x5eed1234u, 0.08, rgba);
    for (int y = S * 10 / 64; y < S * 16 / 64; ++y)            /* a blue band near the top: the set's own mark */
        for (int x = 0; x < S; ++x) memcpy(rgba + ((size_t)y * S + x) * 4, BAND, 3);
    unsigned char *s = blp(S, rgba, len);
    free(rgba);
    return s;
}
/* }}} */

/* {{{ the stand-in model: an eight-sided post a yard across at its foot,
 * 0.8 at its top, two yards high, its sides and top each a flat face;
 * M2 version 264 (one picture, one material, one lookup) and its .skin
 * (one piece, one unit), laid out as 022-m2.c reads them */
#define POST_SIDES 8
#define POST_VERTS (POST_SIDES * 4 + POST_SIDES + 1)
#define POST_TRIS  (POST_SIDES * 2 + POST_SIDES)
static void post_vertex(unsigned char *v, float x, float y, float z, float nx, float ny, float nz, float u, float w)
{
    putf(v, x); putf(v + 4, y); putf(v + 8, z);
    v[12] = 255;                              /* all its weight on bone 0 (none: it stands still) */
    putf(v + 20, nx); putf(v + 24, ny); putf(v + 28, nz);
    putf(v + 32, u); putf(v + 36, w);
}
unsigned char *baseline_stand_in_model(size_t *len)
{
    const char name[] = "W baseline stand-in";
    const char pic[] = BASELINE_STAND_IN_PICTURE;
    const double PI = 3.14159265358979323846;
    const float R0 = 0.5f, R1 = 0.4f, H = 2.0f;
    size_t name_at = 304, pic_at = name_at + sizeof name, verts_at = (pic_at + sizeof pic + 15) & ~(size_t)15;
    size_t tex_at = verts_at + (size_t)POST_VERTS * 48, mat_at = tex_at + 16, look_at = mat_at + 4;
    *len = (look_at + 2 + 15) & ~(size_t)15;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "MD20", 4);
    put32(s + 4, 264);
    put32(s + 8, sizeof name); put32(s + 12, (uint32_t)name_at);
    memcpy(s + name_at, name, sizeof name);
    memcpy(s + pic_at, pic, sizeof pic);
    put32(s + 60, POST_VERTS); put32(s + 64, (uint32_t)verts_at);
    put32(s + 68, 1);                         /* views (skin files): one */
    put32(s + 80, 1); put32(s + 84, (uint32_t)tex_at);     /* one picture, by name (kind 0) */
    put32(s + tex_at + 8, sizeof pic); put32(s + tex_at + 12, (uint32_t)pic_at);
    put32(s + 112, 1); put32(s + 116, (uint32_t)mat_at);   /* one material: flags 0, opaque */
    put32(s + 128, 1); put32(s + 132, (uint32_t)look_at);  /* picture lookup: [0] = 0 */
    unsigned char *v = s + verts_at;
    float slope = (R0 - R1) / H;
    for (int k = 0; k < POST_SIDES; ++k) {
        double a0 = 2 * PI * k / POST_SIDES, a1 = 2 * PI * (k + 1) / POST_SIDES, m = (a0 + a1) / 2;
        float nx = (float)cos(m), ny = (float)sin(m), nl = sqrtf(1 + slope * slope);
        float u0 = (float)k / POST_SIDES, u1 = (float)(k + 1) / POST_SIDES;
        post_vertex(v,       R0 * (float)cos(a0), R0 * (float)sin(a0), 0, nx / nl, ny / nl, slope / nl, u0, 1);
        post_vertex(v + 48,  R0 * (float)cos(a1), R0 * (float)sin(a1), 0, nx / nl, ny / nl, slope / nl, u1, 1);
        post_vertex(v + 96,  R1 * (float)cos(a1), R1 * (float)sin(a1), H, nx / nl, ny / nl, slope / nl, u1, 0);
        post_vertex(v + 144, R1 * (float)cos(a0), R1 * (float)sin(a0), H, nx / nl, ny / nl, slope / nl, u0, 0);
        v += 4 * 48;
    }
    for (int k = 0; k < POST_SIDES; ++k) {    /* the top's ring, then its centre */
        double a = 2 * PI * k / POST_SIDES;
        post_vertex(v, R1 * (float)cos(a), R1 * (float)sin(a), H, 0, 0, 1, 0.5f + 0.5f * (float)cos(a), 0.5f + 0.5f * (float)sin(a));
        v += 48;
    }
    post_vertex(v, 0, 0, H, 0, 0, 1, 0.5f, 0.5f);
    return s;
}
unsigned char *baseline_stand_in_skin(size_t *len)
{
    size_t points_at = 48, tris_at = points_at + (size_t)POST_VERTS * 2, pieces_at = (tris_at + (size_t)POST_TRIS * 3 * 2 + 15) & ~(size_t)15, units_at = pieces_at + 48;
    *len = units_at + 24;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "SKIN", 4);
    put32(s + 4, POST_VERTS); put32(s + 8, (uint32_t)points_at);
    put32(s + 12, POST_TRIS * 3); put32(s + 16, (uint32_t)tris_at);
    put32(s + 28, 1); put32(s + 32, (uint32_t)pieces_at);
    put32(s + 36, 1); put32(s + 40, (uint32_t)units_at);
    for (int i = 0; i < POST_VERTS; ++i) put16(s + points_at + (size_t)i * 2, (uint32_t)i);
    unsigned char *t = s + tris_at;
    for (int k = 0; k < POST_SIDES; ++k) {    /* each side: its foot left to right, then up (counter-clockwise seen from outside) */
        int b = k * 4;
        const int tri[6] = { b, b + 1, b + 2, b, b + 2, b + 3 };
        for (int i = 0; i < 6; ++i, t += 2) put16(t, (uint32_t)tri[i]);
    }
    int ring = POST_SIDES * 4, centre = ring + POST_SIDES;
    for (int k = 0; k < POST_SIDES; ++k) {
        const int tri[3] = { ring + k, ring + (k + 1) % POST_SIDES, centre };
        for (int i = 0; i < 3; ++i, t += 2) put16(t, (uint32_t)tri[i]);
    }
    unsigned char *p = s + pieces_at;         /* the piece: every point and triangle corner */
    put16(p + 6, POST_VERTS); put16(p + 10, POST_TRIS * 3);
    unsigned char *u = s + units_at;          /* the unit: piece 0, no colour, material 0, one layer, picture lookup 0 */
    put16(u + 8, 0xFFFF); put16(u + 14, 1);
    return s;
}
/* }}} */

/* {{{ the stand-in figure: an eight-sided column 0.6 across and 1.8 high
 * (a person's height), turned so a face looks along +x (the way a model
 * faces), that face painted with the set's blue so it shows which way the
 * figure faces; one bone every point hangs from, still, and one
 * animation, Stand (id 0, its moments in the .m2), so 023 reads a
 * skeleton and 030 has a pose to hold */
#define FIG_SIDES 8
#define FIG_VERTS (FIG_SIDES * 4 + FIG_SIDES + 1)
#define FIG_TRIS  (FIG_SIDES * 2 + FIG_SIDES)
unsigned char *baseline_figure_model(size_t *len)
{
    const char name[] = "W baseline stand-in figure";
    const char pic[] = BASELINE_FIGURE_PICTURE;
    const double PI = 3.14159265358979323846, TURN = -PI / FIG_SIDES;   /* a face, not a corner, along +x */
    const float R = 0.3f, H = 1.8f;
    size_t name_at = 304, pic_at = name_at + sizeof name;
    size_t seq_at = (pic_at + sizeof pic + 15) & ~(size_t)15, look_seq_at = seq_at + 64, bone_at = (look_seq_at + 2 + 15) & ~(size_t)15;
    size_t key_at = bone_at + 88, verts_at = (key_at + 2 + 15) & ~(size_t)15;
    size_t tex_at = verts_at + (size_t)FIG_VERTS * 48, mat_at = tex_at + 16, look_at = mat_at + 4, bone_look_at = look_at + 2;
    *len = (bone_look_at + 2 + 15) & ~(size_t)15;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "MD20", 4);
    put32(s + 4, 264);
    put32(s + 8, sizeof name); put32(s + 12, (uint32_t)name_at);
    memcpy(s + name_at, name, sizeof name);
    memcpy(s + pic_at, pic, sizeof pic);
    put32(s + 28, 1); put32(s + 32, (uint32_t)seq_at);       /* one animation: Stand */
    unsigned char *q = s + seq_at;
    put16(q, 0); put16(q + 2, 0); put32(q + 4, 1000);        /* id 0, variation 0, a second long */
    put32(q + 12, 0x20);                                      /* its moments in the .m2 (no .anim file) */
    put16(q + 16, 0x7fff); put32(q + 28, 150);                /* chosen always; blended in over 150 ms */
    putf(q + 52, 1.0f);                                       /* its bounds' radius (unused here) */
    put16(q + 60, 0xffff); put16(q + 62, 0);                  /* no next variation; not an alias */
    put32(s + 36, 1); put32(s + 40, (uint32_t)look_seq_at);  /* animation lookup: Stand is 0 */
    put32(s + 44, 1); put32(s + 48, (uint32_t)bone_at);      /* one bone */
    unsigned char *b = s + bone_at;
    put32(b, 0xffffffffu);                                    /* no key bone */
    put16(b + 8, 0xffff);                                     /* hangs from nothing */
    for (int t = 0; t < 3; ++t) put16(b + 16 + t * 20 + 2, 0xffff);   /* its move, turn and stretch: no global sequence, no moments */
    put32(s + 52, 1); put32(s + 56, (uint32_t)key_at);       /* key bone lookup: none */
    put16(s + key_at, 0xffff);
    put32(s + 60, FIG_VERTS); put32(s + 64, (uint32_t)verts_at);
    put32(s + 68, 1);
    put32(s + 80, 1); put32(s + 84, (uint32_t)tex_at);
    put32(s + tex_at + 8, sizeof pic); put32(s + tex_at + 12, (uint32_t)pic_at);
    put32(s + 112, 1); put32(s + 116, (uint32_t)mat_at);
    put32(s + 120, 1); put32(s + 124, (uint32_t)bone_look_at);   /* bone lookup: [0] = 0 */
    put32(s + 128, 1); put32(s + 132, (uint32_t)look_at);
    unsigned char *v = s + verts_at;
    for (int k = 0; k < FIG_SIDES; ++k) {
        double a0 = TURN + 2 * PI * k / FIG_SIDES, a1 = TURN + 2 * PI * (k + 1) / FIG_SIDES, m = (a0 + a1) / 2;
        float nx = (float)cos(m), ny = (float)sin(m);
        float u0 = (float)k / FIG_SIDES, u1 = (float)(k + 1) / FIG_SIDES;
        post_vertex(v,       R * (float)cos(a0), R * (float)sin(a0), 0, nx, ny, 0, u0, 1);
        post_vertex(v + 48,  R * (float)cos(a1), R * (float)sin(a1), 0, nx, ny, 0, u1, 1);
        post_vertex(v + 96,  R * (float)cos(a1), R * (float)sin(a1), H, nx, ny, 0, u1, 0);
        post_vertex(v + 144, R * (float)cos(a0), R * (float)sin(a0), H, nx, ny, 0, u0, 0);
        v += 4 * 48;
    }
    for (int k = 0; k < FIG_SIDES; ++k) {
        double a = TURN + 2 * PI * k / FIG_SIDES;
        post_vertex(v, R * (float)cos(a), R * (float)sin(a), H, 0, 0, 1, 0.5f, 0.5f);   /* the top: plain (the middle of the picture) */
        v += 48;
    }
    post_vertex(v, 0, 0, H, 0, 0, 1, 0.5f, 0.5f);
    return s;
}
unsigned char *baseline_figure_skin(size_t *len) { return baseline_stand_in_skin(len); }   /* the same corners, triangles, piece and unit as the post's */
unsigned char *baseline_figure_picture(size_t *len)
{
    enum { S = BASELINE_PICTURE_SIDE };
    static const uint8_t SAND[3] = { 184, 152, 112 }, BAND[3] = { 60, 90, 140 };
    unsigned char *rgba = malloc((size_t)S * S * 4);
    mottle(SAND, 0xf16e7e5u, 0.06, rgba);
    for (int y = 0; y < S; ++y)                               /* the face along +x: the first eighth across */
        for (int x = 0; x < S / 8; ++x) memcpy(rgba + ((size_t)y * S + x) * 4, BAND, 3);
    unsigned char *s = blp(S, rgba, len);
    free(rgba);
    return s;
}
/* }}} */

/* {{{ the tables a run reads at its start, as 3.3.5a's widths: LiquidType
 * (one row, water, naming the set's frames), and the light tables empty
 * (no light anywhere: 026's plain grey day) */
typedef struct { uint32_t rows, fields; uint32_t *cells; char *text; size_t text_len; } Table;
static unsigned char *dbc_bytes(const Table *t, size_t *len)
{
    size_t row_len = (size_t)t->fields * 4, cells = (size_t)t->rows * t->fields;
    *len = 20 + cells * 4 + t->text_len;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "WDBC", 4);
    put32(s + 4, t->rows); put32(s + 8, t->fields); put32(s + 12, (uint32_t)row_len); put32(s + 16, (uint32_t)t->text_len);
    for (size_t i = 0; i < cells; ++i) put32(s + 20 + i * 4, t->cells[i]);
    if (t->text_len) memcpy(s + 20 + cells * 4, t->text, t->text_len);
    return s;
}
unsigned char *baseline_table(const char *name, size_t *len)
{
    static const struct { const char *name; uint32_t fields; } EMPTY[] = {
        { "Light", 15 }, { "LightIntBand", 34 }, { "LightFloatBand", 34 }, { "LightParams", 9 }, { "LightSkybox", 3 },
    };
    for (size_t i = 0; i < sizeof EMPTY / sizeof EMPTY[0]; ++i)
        if (!strcmp(name, EMPTY[i].name)) {
            char nul = '\0';
            Table t = { 0, EMPTY[i].fields, NULL, &nul, 1 };
            return dbc_bytes(&t, len);
        }
    if (!strcmp(name, "LiquidType")) {
        /* 1 id, 2 name, 3 flags, 4 type (0 water), 16 the first picture
         * name, %d the frame; the rest 0. Its text: "", "Water", the pattern */
        char text[200];
        size_t water = 1, pattern = water + sizeof "Water";
        memset(text, 0, sizeof text);
        memcpy(text + water, "Water", sizeof "Water");
        memcpy(text + pattern, BASELINE_WATER_PATTERN, sizeof BASELINE_WATER_PATTERN);
        uint32_t cells[BASELINE_LIQUID_FIELDS] = { 0 };
        cells[0] = 1; cells[1] = (uint32_t)water; cells[15] = (uint32_t)pattern;
        Table t = { 1, BASELINE_LIQUID_FIELDS, cells, text, pattern + sizeof BASELINE_WATER_PATTERN };
        return dbc_bytes(&t, len);
    }
    *len = 0;
    return NULL;
}
const char *const BASELINE_TABLES[] = { "LiquidType", "Light", "LightIntBand", "LightFloatBand", "LightParams", "LightSkybox", NULL };
/* }}} */

/* {{{ baseline_sha256_hex (FIPS 180-4) */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
static uint32_t ror(uint32_t x, int n) { return x >> n | x << (32 - n); }
static void block(uint32_t h[8], const unsigned char *b)
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = (uint32_t)b[i * 4] << 24 | (uint32_t)b[i * 4 + 1] << 16 | (uint32_t)b[i * 4 + 2] << 8 | b[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ w[i - 15] >> 3, s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ w[i - 2] >> 10;
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = bb; bb = a; a = t1 + t2;
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
void baseline_sha256_hex(const void *data, size_t len, char out[65])
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    const unsigned char *p = data;
    size_t whole = len / 64 * 64;
    for (size_t i = 0; i < whole; i += 64) block(h, p + i);
    unsigned char tail[128] = { 0 };
    size_t rest = len - whole, n = rest < 56 ? 64 : 128;
    if (rest) memcpy(tail, p + whole, rest);
    tail[rest] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i) tail[n - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t i = 0; i < n; i += 64) block(h, tail + i);
    for (int i = 0; i < 8; ++i) snprintf(out + i * 8, 9, "%08x", h[i]);
}
/* }}} */

/* {{{ the making */
typedef struct { char *replaces; char key[1024]; uint8_t rgb[3]; } Row;
typedef struct {
    Row  *rows; int n, cap;
    char *text; size_t len, cap_text;      /* the manifest, as it is written */
    BaselineMade *made;
} Make;

static void add_row(Make *mk, const char *replaces, const char *key, const uint8_t rgb[3])
{
    if (mk->n == mk->cap) { mk->cap = mk->cap ? mk->cap * 2 : 64; mk->rows = realloc(mk->rows, sizeof *mk->rows * (size_t)mk->cap); }
    Row *r = &mk->rows[mk->n++];
    r->replaces = strdup(replaces);
    snprintf(r->key, sizeof r->key, "%s", key);
    if (rgb) memcpy(r->rgb, rgb, 3); else memset(r->rgb, 0, 3);
}
static int have_row(const Make *mk, const char *key)
{
    for (int i = 0; i < mk->n; ++i) if (!strcmp(mk->rows[i].key, key)) return 1;
    return 0;
}

/* a row's path: `wow`, text and not empty, folded into `key`; a ".." part
 * (which would write outside the folder) or the manifest's own name refused */
static int climbs(const char *key)
{
    for (const char *p = key; (p = strstr(p, "..")) != NULL; p += 2)
        if ((p == key || p[-1] == '/') && (p[2] == '/' || !p[2])) return 1;
    return !strcmp(key, "manifest.lua") || !strcmp(key, "manifest.lua.new");
}
static WErr row_path(const LuaValue *row, const char *where, const char **path, char key[1024], WWhy *why)
{
    const LuaValue *w = lua_value_field(row, "wow");
    *path = lua_value_text(w);
    if (!*path || fs_normalise(*path, key, 1024) || climbs(key))
        return w_fail(why, W_ERR_REFUSED, "%s line %d: a row's wow is %s, not a file's path in the set", where, lua_value_line(w),
                      !*path ? lua_type_name(lua_value_type(w)) : climbs(key) ? "a path that climbs out of the folder or names the manifest" : "empty or too long");
    return W_OK;
}

/* {{{ static grounds: every table with a `wow` under `t` is a ground row */
static WErr grounds(Make *mk, const LuaValue *t, const char *where, WWhy *why)
{
    for (int i = 1; i <= lua_value_count(t); ++i) {
        const LuaValue *v = lua_value_at(t, i, NULL);
        if (lua_value_type(v) != LUA_DATA_TABLE) continue;
        if (!lua_value_field(v, "wow")) { WErr rc = grounds(mk, v, where, why); if (rc) return rc; continue; }
        const char *path;
        char key[1024];
        WErr rc = row_path(v, where, &path, key, why);
        if (rc) return rc;
        const LuaValue *c = lua_value_field(v, "rgb");
        uint8_t rgb[3];
        int ok = lua_value_type(c) == LUA_DATA_TABLE && lua_value_listed(c) == 3;
        for (int k = 0; ok && k < 3; ++k) {
            const LuaValue *n = lua_value_item(c, k + 1);
            double x = lua_value_number(n);
            ok = lua_value_type(n) == LUA_DATA_NUMBER && x >= 0 && x <= 255 && x == floor(x);
            if (ok) rgb[k] = (uint8_t)x;
        }
        if (!ok) return w_fail(why, W_ERR_REFUSED, "%s line %d: the ground row for %s has no rgb of three whole numbers from 0 to 255", where, lua_value_line(v), path);
        if (have_row(mk, key)) { ++mk->made->same_path; continue; }
        add_row(mk, path, key, rgb);
    }
    return W_OK;
}
/* }}} */

/* {{{ static models: every table with a `wow` under `t` is a model row; under `units`, counted and left */
static int ends_with(const char *s, const char *tail)
{
    size_t n = strlen(s), t = strlen(tail);
    if (t > n) return 0;
    for (size_t i = 0; i < t; ++i) if ((s[n - t + i] | 0x20) != (tail[i] | 0x20)) return 0;
    return 1;
}
static WErr models(Make *mk, const LuaValue *t, const char *where, int units, WWhy *why)
{
    for (int i = 1; i <= lua_value_count(t); ++i) {
        const char *k;
        const LuaValue *v = lua_value_at(t, i, &k);
        if (lua_value_type(v) != LUA_DATA_TABLE) continue;
        if (!lua_value_field(v, "wow")) {
            WErr rc = models(mk, v, where, units || (k && !strcmp(k, "units")), why);
            if (rc) return rc;
            continue;
        }
        const char *path;
        char key[1024];
        WErr rc = row_path(v, where, &path, key, why);
        if (rc) return rc;
        if (units) { ++mk->made->units; continue; }
        if (ends_with(key, ".wmo")) { ++mk->made->buildings; continue; }
        if (!ends_with(key, ".m2")) { ++mk->made->other; continue; }
        if (have_row(mk, key)) { ++mk->made->same_path; continue; }
        add_row(mk, path, key, NULL);
    }
    return W_OK;
}
/* }}} */

/* {{{ writing: folders made as needed, a file whole, and the manifest's text */
static WErr mkdirs(const char *path, WWhy *why)
{
    char p[1200];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; ; ++c) {
        if (*c == '/' || !*c) {
            char was = *c;
            *c = '\0';
            if (mkdir(p, 0755) != 0 && errno != EEXIST) return w_fail(why, W_ERR_IO, "can't make the folder %s: %s", p, strerror(errno));
            *c = was;
            if (!was) break;
        }
    }
    return W_OK;
}
static WErr write_file(const char *folder, const char *name, const unsigned char *s, size_t len, WWhy *why)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", folder, name);
    char *slash = strrchr(path, '/');
    *slash = '\0';
    WErr rc = mkdirs(path, why);
    *slash = '/';
    if (rc) return rc;
    FILE *f = fopen(path, "wb");
    if (!f) return w_fail(why, W_ERR_IO, "can't write %s: %s", path, strerror(errno));
    size_t put = fwrite(s, 1, len, f);
    if (fclose(f) != 0 || put != len) return w_fail(why, W_ERR_IO, "can't write all of %s", path);
    return W_OK;
}
static void say(Make *mk, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void say(Make *mk, const char *fmt, ...)
{
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        size_t room = mk->cap_text - mk->len;
        int n = vsnprintf(mk->text ? mk->text + mk->len : NULL, mk->text ? room : 0, fmt, ap);
        va_end(ap);
        if (n >= 0 && mk->text && (size_t)n < room) { mk->len += (size_t)n; return; }
        mk->cap_text = (mk->cap_text + (size_t)(n > 0 ? n : 0) + 1) * 2;
        mk->text = realloc(mk->text, mk->cap_text);
    }
}
/* a Lua string: backslashes and quotes escaped */
static void say_text(Make *mk, const char *s)
{
    say(mk, "\"");
    for (; *s; ++s) {
        if (*s == '\\' || *s == '"') say(mk, "\\%c", *s);
        else say(mk, "%c", *s);
    }
    say(mk, "\"");
}
static void say_entry(Make *mk, const char *replaces, const char *file, const char *kind, const char *id)
{
    say(mk, "        { replaces = ");
    say_text(mk, replaces);
    say(mk, ", file = ");
    say_text(mk, file);
    say(mk, ", kind = \"%s\", id = \"%s\", route = \"hand\", lineage = \"independent\" },\n", kind, id);
    ++mk->made->entries;
}
/* writes `s` at `file` and lists it as `replaces` */
static WErr put(Make *mk, const char *folder, const char *replaces, const char *file, const char *kind, unsigned char *s, size_t len, WWhy *why)
{
    char id[65];
    baseline_sha256_hex(s, len, id);
    WErr rc = write_file(folder, file, s, len, why);
    free(s);
    if (rc) return rc;
    ++mk->made->files;
    say_entry(mk, replaces, file, kind, id);
    return W_OK;
}
/* }}} */

static uint32_t path_seed(const char *key) { uint32_t h = 2166136261u; while (*key) h = (h ^ (unsigned char)*key++) * 16777619u; return h; }

WErr baseline_set_make(const char *folder, const char *ground_table, const char *model_table, BaselineMade *made, WWhy *why)
{
    memset(made, 0, sizeof *made);
    Make mk = { .made = made };
    LuaData *gd = NULL, *md = NULL;
    WErr rc;
    /* a folder already holding files but no manifest is not one this made: left alone */
    struct stat st;
    if (stat(folder, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) return w_fail(why, W_ERR_REFUSED, "%s is there and isn't a folder", folder);
        FsIndex *ix;
        if ((rc = fs_index_folder(folder, &ix, why))) return rc;
        int files = fs_index_count(ix), manifest = fs_index_find(ix, "manifest.lua") != NULL;
        fs_index_free(ix);
        if (files && !manifest)
            return w_fail(why, W_ERR_REFUSED, "%s holds %d file%s and no manifest.lua, so it isn't a baseline set: left alone (give an empty or new folder)", folder, files, files == 1 ? "" : "s");
    }
    if ((rc = lua_data_read(ground_table, TABLE_MAX, &gd, why))) goto done;
    if ((rc = lua_data_read(model_table, TABLE_MAX, &md, why))) goto done;
    if ((rc = grounds(&mk, lua_data_top(gd), ground_table, why))) goto done;
    int ngrounds = mk.n;
    if ((rc = models(&mk, lua_data_top(md), model_table, 0, why))) goto done;
    if ((rc = mkdirs(folder, why))) goto done;

    say(&mk, "-- manifest.lua: the smallest baseline set, made by make-baseline (085-baseline-set.c)\n"
             "-- from the converter's %s and %s; every file made by the client's own code.\n"
             "return {\n    format = 1,\n    name = \"The smallest baseline set\",\n    entries = {\n",
        strrchr(ground_table, '/') ? strrchr(ground_table, '/') + 1 : ground_table, strrchr(model_table, '/') ? strrchr(model_table, '/') + 1 : model_table);
    /* the ground: each at its own name */
    for (int i = 0; i < ngrounds && !rc; ++i) {
        size_t len;
        unsigned char *s = baseline_ground_picture(mk.rows[i].rgb, path_seed(mk.rows[i].key), &len);
        if (!(rc = put(&mk, folder, mk.rows[i].replaces, mk.rows[i].key, "texture", s, len, why))) ++made->grounds;
    }
    /* the stand-in: its picture and its two files, then every model's name on them */
    if (!rc && mk.n > ngrounds) {
        size_t len;
        char key[1024];
        fs_normalise(BASELINE_STAND_IN_PICTURE, key, sizeof key);
        unsigned char *s = baseline_stand_in_picture(&len);
        rc = put(&mk, folder, BASELINE_STAND_IN_PICTURE, key, "texture", s, len, why);
        char m2_id[65], skin_id[65];
        if (!rc) {
            s = baseline_stand_in_model(&len);
            baseline_sha256_hex(s, len, m2_id);
            rc = write_file(folder, BASELINE_STAND_IN_MODEL, s, len, why);
            free(s);
        }
        if (!rc) {
            s = baseline_stand_in_skin(&len);
            baseline_sha256_hex(s, len, skin_id);
            rc = write_file(folder, BASELINE_STAND_IN_SKIN, s, len, why);
            free(s);
        }
        if (!rc) made->files += 2;
        for (int i = ngrounds; i < mk.n && !rc; ++i) {
            const char *m2 = mk.rows[i].replaces;
            char skin[1100];
            snprintf(skin, sizeof skin, "%.*s00.skin", (int)(strlen(m2) - 3), m2);
            say_entry(&mk, m2, BASELINE_STAND_IN_MODEL, "model", m2_id);
            say_entry(&mk, skin, BASELINE_STAND_IN_SKIN, "model", skin_id);
            ++made->models;
        }
    }
    /* the stand-in figure (for her and the creatures, when no place has their model) */
    if (!rc) {
        size_t len;
        char key[1024];
        fs_normalise(BASELINE_FIGURE_PICTURE, key, sizeof key);
        unsigned char *s = baseline_figure_picture(&len);
        rc = put(&mk, folder, BASELINE_FIGURE_PICTURE, key, "texture", s, len, why);
        if (!rc) { fs_normalise(BASELINE_FIGURE_MODEL, key, sizeof key); s = baseline_figure_model(&len); rc = put(&mk, folder, BASELINE_FIGURE_MODEL, key, "model", s, len, why); }
        if (!rc) { fs_normalise(BASELINE_FIGURE_SKIN, key, sizeof key); s = baseline_figure_skin(&len); rc = put(&mk, folder, BASELINE_FIGURE_SKIN, key, "model", s, len, why); }
    }
    /* the tables a start reads */
    for (int t = 0; BASELINE_TABLES[t] && !rc; ++t) {
        char name[200], key[200];
        snprintf(name, sizeof name, "DBFilesClient\\%s.dbc", BASELINE_TABLES[t]);
        fs_normalise(name, key, sizeof key);
        size_t len;
        unsigned char *s = baseline_table(BASELINE_TABLES[t], &len);
        if (!(rc = put(&mk, folder, name, key, "table", s, len, why))) ++made->tables;
    }
    /* the water's run */
    for (int f = 1; f <= BASELINE_WATER_FRAMES && !rc; ++f) {
        char name[200], key[200];
        snprintf(name, sizeof name, BASELINE_WATER_PATTERN, f);
        fs_normalise(name, key, sizeof key);
        size_t len;
        unsigned char *s = baseline_water_frame(f, &len);
        if (!(rc = put(&mk, folder, name, key, "texture", s, len, why))) ++made->water_frames;
    }
    if (!rc) {
        say(&mk, "    },\n}\n");
        /* the manifest last, through a new name, so a make that stops part way leaves the last one whole */
        char tmp[1200], final[1200];
        snprintf(tmp, sizeof tmp, "%s/manifest.lua.new", folder);
        snprintf(final, sizeof final, "%s/manifest.lua", folder);
        rc = write_file(folder, "manifest.lua.new", (const unsigned char *)mk.text, mk.len, why);
        if (!rc && rename(tmp, final) != 0) rc = w_fail(why, W_ERR_IO, "can't put the manifest in place at %s: %s", final, strerror(errno));
    }
done:
    for (int i = 0; i < mk.n; ++i) free(mk.rows[i].replaces);
    free(mk.rows);
    free(mk.text);
    lua_data_free(gd);
    lua_data_free(md);
    return rc;
}
/* }}} */
