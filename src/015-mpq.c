/*
 * 015-mpq.c (issue 173k) - see 015-mpq.h. Ported from src/custom-client/lib/mpq.lua.
 */
#include "015-mpq.h"
#include "013-errors.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <ctype.h>
#include <dirent.h>
#include <strings.h>
#include <time.h>

/* block flags (the block table's flags field) */
#define FILE_IMPLODE     0x00000100u
#define FILE_COMPRESS    0x00000200u
#define FILE_ENCRYPTED   0x00010000u
#define FILE_FIX_KEY     0x00020000u
#define FILE_SINGLE_UNIT 0x01000000u
#define FILE_EXISTS      0x80000000u
#define HASH_EMPTY       0xFFFFFFFFu

/* {{{ the scrambling table
 * 1280 words made by a fixed recipe every MPQ reader builds: five rows of
 * 256 (index slot, two name checks, file key, decryption). Not state: a
 * constant, worked out once on first use (by whichever thread asks first,
 * the others waiting) and never changed after, so every chain shares it. */
static uint32_t CRYPT[1280];
static pthread_once_t crypt_once = PTHREAD_ONCE_INIT;
static void crypt_make(void)
{
    uint32_t seed = 0x00100001;
    for (int i1 = 0; i1 < 256; ++i1) {
        int i2 = i1;
        for (int i = 0; i < 5; ++i) {
            seed = (seed * 125 + 3) % 0x2AAAAB;
            uint32_t hi = (seed & 0xFFFF) << 16;
            seed = (seed * 125 + 3) % 0x2AAAAB;
            uint32_t lo = seed & 0xFFFF;
            CRYPT[i2] = hi | lo;
            i2 += 256;
        }
    }
}
static void crypt_init(void) { pthread_once(&crypt_once, crypt_make); }
/* }}} */

/* {{{ mpq_hash
 * The name hash of `text` for purpose `kind`: case-insensitive, either slash. */
uint32_t mpq_hash(const char *text, int kind)
{
    crypt_init();
    uint32_t seed1 = 0x7FED7FED, seed2 = 0xEEEEEEEE;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        uint32_t ch = *p;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == '/') ch = '\\';
        seed1 = CRYPT[kind * 256 + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}
/* }}} */

/* {{{ static decrypt */
static void decrypt(uint32_t *w, size_t count, uint32_t key)
{
    crypt_init();
    uint32_t seed = 0xEEEEEEEE;
    for (size_t i = 0; i < count; ++i) {
        seed += CRYPT[0x400 + (key & 0xFF)];
        uint32_t ch = w[i] ^ (key + seed);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        seed = ch + seed + (seed << 5) + 3;
        w[i] = ch;
    }
}
/* }}} */

/* {{{ static rd32 */
static uint32_t rd32(const unsigned char *s) { return (uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24); }
/* }}} */

/* {{{ mpq_open */
WErr mpq_open(MpqArchive *a, const char *path, WWhy *why)
{
    memset(a, 0, sizeof *a);
    snprintf(a->path, sizeof a->path, "%s", path);
    a->file = fopen(path, "rb");
    if (!a->file) { w_fail(why, W_ERR_IO, "can't open %s", path); return why->code; }
    unsigned char head[44];
    int found = 0;
    /* the header sits at the start, or at a later multiple of 512 */
    for (uint64_t at = 0; at <= 64ull * 1024 * 1024; at += 512) {
        if (fseeko(a->file, (off_t)at, SEEK_SET) != 0) break;
        size_t got = fread(head, 1, 44, a->file);
        if (got < 32) break;
        if (memcmp(head, "MPQ\x1a", 4) == 0) { a->base = at; found = 1; break; }
    }
    if (!found) { w_fail(why, W_ERR_CORRUPT, "%s is not an MPQ archive (no MPQ mark)", path); fclose(a->file); a->file = NULL; return why->code; }

    a->sector = 512u << (head[14] | (head[15] << 8));
    uint64_t hash_at = rd32(head + 16), block_at = rd32(head + 20);
    a->nhash = rd32(head + 24);
    a->nblock = rd32(head + 28);
    int version = head[12] | (head[13] << 8);
    if (version >= 1) {                                  /* v2: the tables' upper 16 bits */
        hash_at  += (uint64_t)(head[40] | (head[41] << 8)) << 32;
        block_at += (uint64_t)(head[42] | (head[43] << 8)) << 32;
    }
    a->hashes = malloc((size_t)a->nhash * 16);
    a->blocks = malloc((size_t)a->nblock * 16);
    fseeko(a->file, (off_t)(a->base + hash_at), SEEK_SET);
    if (fread(a->hashes, 16, a->nhash, a->file) != a->nhash) { w_fail(why, W_ERR_CORRUPT, "%s: short index", path); return why->code; }
    fseeko(a->file, (off_t)(a->base + block_at), SEEK_SET);
    if (fread(a->blocks, 16, a->nblock, a->file) != a->nblock) { w_fail(why, W_ERR_CORRUPT, "%s: short block list", path); return why->code; }
    decrypt(a->hashes, (size_t)a->nhash * 4, mpq_hash("(hash table)", 3));
    decrypt(a->blocks, (size_t)a->nblock * 4, mpq_hash("(block table)", 3));
    return 0;
}
/* }}} */

/* {{{ static find_block */
static long find_block(MpqArchive *a, const char *name)
{
    uint32_t slot = mpq_hash(name, 0) % a->nhash;
    uint32_t na = mpq_hash(name, 1), nb = mpq_hash(name, 2);
    for (uint32_t n = 0; n < a->nhash; ++n) {
        uint32_t *h = a->hashes + (size_t)slot * 4;
        if (h[3] == HASH_EMPTY) return -1;
        if (h[0] == na && h[1] == nb && h[3] < a->nblock) return (long)h[3];
        slot = (slot + 1) % a->nhash;
    }
    return -1;
}
/* }}} */

/* {{{ static inflate_piece
 * One compressed piece: its first byte names the compression; only zlib (0x02). */
static WErr inflate_piece(const unsigned char *raw, size_t len, unsigned char *out, size_t want, const char *name, WWhy *why)
{
    if (raw[0] != 0x02) { w_fail(why, W_ERR_UNSUPPORTED, "%s: compression 0x%02x is not read by this reader (zlib only)", name, raw[0]); return why->code; }
    uLongf got = want;
    int rc = uncompress(out, &got, raw + 1, len - 1);
    if (rc != Z_OK || got != want) { w_fail(why, W_ERR_CORRUPT, "%s: zlib failed (%d)", name, rc); return why->code; }
    return 0;
}
/* }}} */

/* {{{ static unscramble
 * Unscrambles whole words of `buf` with `key`; a last partial word stays as it is. */
static void unscramble(unsigned char *buf, size_t len, uint32_t key)
{
    size_t n = len / 4;
    uint32_t *w = malloc(n * 4 + 4);
    memcpy(w, buf, n * 4);
    decrypt(w, n, key);
    memcpy(buf, w, n * 4);
    free(w);
}
/* }}} */

/* {{{ mpq_read */
unsigned char *mpq_read(MpqArchive *a, const char *name, size_t *size, WWhy *why)
{
    long bi = find_block(a, name);
    if (bi < 0) { w_fail(why, W_ERR_NOT_FOUND, "%s is not in %s", name, a->path); return NULL; }
    uint32_t *b = a->blocks + bi * 4;
    uint32_t pos = b[0], csize = b[1], fsize = b[2], flags = b[3];
    if (!(flags & FILE_EXISTS)) { w_fail(why, W_ERR_NOT_FOUND, "%s is listed in %s as deleted", name, a->path); return NULL; }
    if (flags & FILE_IMPLODE) { w_fail(why, W_ERR_UNSUPPORTED, "%s: PKWARE implode is not read by this reader", name); return NULL; }

    int encrypted = (flags & FILE_ENCRYPTED) != 0;
    uint32_t key = 0;
    if (encrypted) {
        const char *base = name;
        for (const char *p = name; *p; ++p) if (*p == '\\' || *p == '/') base = p + 1;
        key = mpq_hash(base, 3);
        if (flags & FILE_FIX_KEY) key = (key + pos) ^ fsize;
    }
    int compressed = (flags & FILE_COMPRESS) != 0;
    unsigned char *out = malloc(fsize ? fsize : 1);
    *size = fsize;
    fseeko(a->file, (off_t)(a->base + pos), SEEK_SET);

    /* one piece: the whole file at once */
    if (flags & FILE_SINGLE_UNIT) {
        unsigned char *raw = malloc(csize ? csize : 1);
        if (fread(raw, 1, csize, a->file) != csize) { free(raw); free(out); w_fail(why, W_ERR_CORRUPT, "%s: short read", name); return NULL; }
        if (encrypted) unscramble(raw, csize, key);
        if (compressed && csize < fsize) {
            if (inflate_piece(raw, csize, out, fsize, name, why)) { free(raw); free(out); return NULL; }
        } else memcpy(out, raw, fsize);
        free(raw);
        return out;
    }
    uint32_t n = (fsize + a->sector - 1) / a->sector;
    if (!compressed) {
        if (fread(out, 1, fsize, a->file) != fsize) { free(out); w_fail(why, W_ERR_CORRUPT, "%s: short read", name); return NULL; }
        if (encrypted)
            for (uint32_t i = 0; i < n; ++i) {
                size_t from = (size_t)i * a->sector, len = fsize - from < a->sector ? fsize - from : a->sector;
                unscramble(out + from, len, key + i);
            }
        return out;
    }
    /* sectors: a table of where each one starts, then each sector */
    uint32_t *offsets = malloc(((size_t)n + 1) * 4);
    if (fread(offsets, 4, (size_t)n + 1, a->file) != (size_t)n + 1) { free(offsets); free(out); w_fail(why, W_ERR_CORRUPT, "%s: short sector table", name); return NULL; }
    if (encrypted) decrypt(offsets, (size_t)n + 1, key - 1);
    unsigned char *raw = malloc(a->sector + 64);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t from = offsets[i], len = offsets[i + 1] - from;
        size_t want = fsize - (size_t)i * a->sector < a->sector ? fsize - (size_t)i * a->sector : a->sector;
        if (len > a->sector + 64) { free(raw); free(offsets); free(out); w_fail(why, W_ERR_CORRUPT, "%s: sector %u is %u bytes", name, i, len); return NULL; }
        fseeko(a->file, (off_t)(a->base + pos + from), SEEK_SET);
        if (fread(raw, 1, len, a->file) != len) { free(raw); free(offsets); free(out); w_fail(why, W_ERR_CORRUPT, "%s: short sector", name); return NULL; }
        if (encrypted) unscramble(raw, len, key + i);
        if (len < want) {
            if (inflate_piece(raw, len, out + (size_t)i * a->sector, want, name, why)) { free(raw); free(offsets); free(out); return NULL; }
        } else memcpy(out + (size_t)i * a->sector, raw, want);
    }
    free(raw);
    free(offsets);
    return out;
}
/* }}} */

/* {{{ mpq_close */
void mpq_close(MpqArchive *a)
{
    if (a->file) fclose(a->file);
    free(a->hashes);
    free(a->blocks);
    memset(a, 0, sizeof *a);
}
/* }}} */

/* {{{ the chain
 * The 3.3.5a client's archives in its search order, newest first (a file
 * in a patch replaces the same name in the base archives). */
static const char *const ORDER[] = {
    "%s/patch-3.MPQ", "%s/patch-2.MPQ", "%s/patch.MPQ",
    "%s/%s/patch-%s-3.MPQ", "%s/%s/patch-%s-2.MPQ", "%s/%s/patch-%s.MPQ",
    "%s/lichking.MPQ", "%s/expansion.MPQ", "%s/common-2.MPQ", "%s/common.MPQ",
    "%s/%s/lichking-locale-%s.MPQ", "%s/%s/expansion-locale-%s.MPQ", "%s/%s/locale-%s.MPQ",
};
WErr mpq_chain_open(MpqChain *c, const char *data, const char *locale, WWhy *why)
{
    memset(c, 0, sizeof *c);
    pthread_mutex_init(&c->lock, NULL);
    for (size_t i = 0; i < sizeof ORDER / sizeof ORDER[0]; ++i) {
        char path[600];
        /* each pattern names the folder, then (for locale archives) the locale twice */
        snprintf(path, sizeof path, ORDER[i], data, locale, locale);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fclose(f);
        MpqArchive *a = malloc(sizeof *a);
        if (mpq_open(a, path, why)) { free(a); mpq_chain_close(c); return why->code; }
        c->archives[c->count++] = a;
    }
    if (c->count == 0) { w_fail(why, W_ERR_NOT_FOUND, "no MPQ archives found in %s", data); return why->code; }
    return 0;
}

/* {{{ mpq_chain_open_wc3
 * Warcraft III's archives in its own search order, newest first: the
 * patch, the expansion's localised art, the expansion, the base game's
 * localised art, the base game. An install's names differ in capitals
 * (war3.mpq beside War3x.mpq), so each is found by name in the folder
 * without regard to case. */
static const char *const WC3_ORDER[] = { "War3Patch.mpq", "War3xLocal.mpq", "War3x.mpq", "War3Local.mpq", "war3.mpq" };
WErr mpq_chain_open_wc3(MpqChain *c, const char *folder, WWhy *why)
{
    memset(c, 0, sizeof *c);
    pthread_mutex_init(&c->lock, NULL);
    DIR *d = opendir(folder);
    if (!d) { w_fail(why, W_ERR_NOT_FOUND, "no Warcraft III install at %s (the folder isn't there)", folder); return why->code; }
    char found[sizeof WC3_ORDER / sizeof WC3_ORDER[0]][256] = { { 0 } };
    for (struct dirent *e; (e = readdir(d)); )
        for (size_t i = 0; i < sizeof WC3_ORDER / sizeof WC3_ORDER[0]; ++i)
            if (!found[i][0] && !strcasecmp(e->d_name, WC3_ORDER[i])) snprintf(found[i], sizeof found[i], "%s", e->d_name);
    closedir(d);
    for (size_t i = 0; i < sizeof WC3_ORDER / sizeof WC3_ORDER[0]; ++i) {
        if (!found[i][0]) continue;
        char path[600];
        int n = snprintf(path, sizeof path, "%s/%s", folder, found[i]);
        if (n < 0 || (size_t)n >= sizeof path) { mpq_chain_close(c); w_fail(why, W_ERR_REFUSED, "the Warcraft III folder's name is too long: %.80s...", folder); return why->code; }
        MpqArchive *a = malloc(sizeof *a);
        if (mpq_open(a, path, why)) { free(a); mpq_chain_close(c); return why->code; }
        c->archives[c->count++] = a;
    }
    if (c->count == 0) { w_fail(why, W_ERR_NOT_FOUND, "no Warcraft III archives (war3.mpq, War3x.mpq, War3Patch.mpq ...) in %s", folder); return why->code; }
    return 0;
}
/* }}} */
/* {{{ static same_name: archive names match without regard to case, and
 * either slash */
static int same_name(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        char x = *a == '/' ? '\\' : (char)toupper((unsigned char)*a), y = *b == '/' ? '\\' : (char)toupper((unsigned char)*b);
        if (x != y) return 0;
    }
    return *a == *b;
}
/* }}} */
static unsigned char *chain_read(MpqChain *c, const char *name, size_t *size, const char **from);
unsigned char *mpq_chain_read(MpqChain *c, const char *name, size_t *size, const char **from)
{
    pthread_mutex_lock(&c->lock);
    unsigned char *s = chain_read(c, name, size, from);
    pthread_mutex_unlock(&c->lock);
    return s;
}
static unsigned char *chain_read(MpqChain *c, const char *name, size_t *size, const char **from)
{
    ++c->tick;
    for (int k = 0; k < c->nkept; ++k)
        if (same_name(c->kept[k].name, name)) {
            unsigned char *copy = malloc(c->kept[k].len ? c->kept[k].len : 1);
            memcpy(copy, c->kept[k].data, c->kept[k].len);
            *size = c->kept[k].len;
            if (from) *from = c->kept[k].from;
            c->kept[k].used = c->tick;
            ++c->hits;
            return copy;
        }
    ++c->misses;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    unsigned char *got = NULL;
    WWhy why;
    for (int i = 0; i < c->count; ++i) {
        unsigned char *s = mpq_read(c->archives[i], name, size, &why);
        if (s) {
            if (from) *from = c->archives[i]->path;
            /* remembered, if it fits the budget once older ones are forgotten
             * (a file bigger than a quarter of it isn't kept: a ground file
             * read once shouldn't push out a hundred tables) */
            if (*size <= MPQ_KEEP_BYTES / 4 && strlen(name) < sizeof c->kept[0].name) {
                while (c->kept_bytes + *size > MPQ_KEEP_BYTES && c->nkept > 0) {
                    int old = 0;
                    for (int k = 1; k < c->nkept; ++k) if (c->kept[k].used < c->kept[old].used) old = k;
                    c->kept_bytes -= c->kept[old].len;
                    free(c->kept[old].data);
                    c->kept[old] = c->kept[--c->nkept];
                }
                if (c->nkept == c->kept_cap) { c->kept_cap = c->kept_cap ? c->kept_cap * 2 : 256; c->kept = realloc(c->kept, sizeof *c->kept * c->kept_cap); }
                MpqKept *kp = &c->kept[c->nkept++];
                snprintf(kp->name, sizeof kp->name, "%s", name);
                kp->data = malloc(*size ? *size : 1);
                memcpy(kp->data, s, *size);
                kp->len = *size;
                kp->used = c->tick;
                kp->from = c->archives[i]->path;
                c->kept_bytes += *size;
            }
            got = s;
            break;
        }
        if (why.code != W_ERR_NOT_FOUND) { fprintf(stderr, "mpq: %s\n", why.text); break; }   /* not in this archive: the next one down; anything else is a broken file */
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    c->unpack_s += (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    return got;
}
void mpq_chain_close(MpqChain *c)
{
    for (int i = 0; i < c->count; ++i) { mpq_close(c->archives[i]); free(c->archives[i]); }
    for (int k = 0; k < c->nkept; ++k) free(c->kept[k].data);
    free(c->kept);
    c->kept = NULL; c->nkept = c->kept_cap = 0; c->kept_bytes = 0;
    c->count = 0;
}
/* }}} */
